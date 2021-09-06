#include <fstream>
#include <memory>
#include <set>
#include <unordered_set>
#include <iterator>

#include <sys/ioctl.h>

#include "termcolor/termcolor.hpp"
#include "argparse/argparse.hpp"
#include "execution.h"
#include "test_path.h"
#include "test_runner.h"
#include "test_result.h"
#include "string_helper.h"

struct winsize get_winsize() {
  struct winsize winsize;
  ioctl(0, TIOCGWINSZ, &winsize);
  return winsize;
}

int main(int argc, char *argv[]) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;

  std::cout << termcolor::reset;
  std::cerr << termcolor::reset;

  argparse::ArgumentParser program("pincheck", "210902");
  program.add_argument("-p", "--project")
         .help("Pintos project to run test; threads, userprog, vm, or filesys");
  program.add_argument("-j", "--jobs")
         .help("Maximum number of parallel test execution; "s + std::to_string(HARDWARE_CONCURRENCY) + " by default for this machine")
         .scan<'i', unsigned>()
         .default_value(HARDWARE_CONCURRENCY);
  program.add_argument("--verbose", "-V")
         .help("Verbose print")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-cb", "--clean-build")
         .help("Clean build directory before test")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-t", "--test", "--")
         .help("Test name; can be given multiple times")
         .default_value(Vector<String>{"*"})
         .append();
  program.add_argument("-sd", "--subdir")
         .help("Test subdir; can be given multiple times")
         .default_value(Vector<String>{"*"})
         .append();
  program.add_argument("-e", "--exclude")
         .help("Test name to be excluded; can be given multiple times")
         .default_value(Vector<String>{})
         .append();
  program.add_argument("-sde", "--subdir-exclude")
         .help("Test subdir to be excluded; can be given multiple times")
         .default_value(Vector<String>{})
         .append();
  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& e) {
    panic_msg << "Arguments parsing failed: " << e.what();
    panic(panic_msg);
  }
  const auto is_verbose = program.get<bool>("--verbose");
  TestPath paths;
  detect_src(paths);
  if(is_verbose) {
    std::cout << "Pintos Src: " << std::string{paths.src} << std::endl;
  }

  if(program.is_used("--project")) {
    paths.project = program.get<String>("--project");
  } else {
    detect_project(paths);
  }
  constexpr std::array<const char*, 4> all_proj = {"threads", "userprog", "vm", "filesys"};
  if(std::find(all_proj.cbegin(), all_proj.cend(), paths.project) == all_proj.cend()) {
    panic_msg << "The detected or given project name is not thread, userprog, vm, nor filesys.\n";
    panic_msg << "Please move path to any project directory or its build directory.";
    panic(panic_msg);
  }
  if(is_verbose) {
    std::cout << "Pintos Project: " << paths.project << std::endl;
  }

  detect_build(paths, is_verbose, program.get<bool>("--clean-build"));
  if(is_verbose) {
    std::cout << "Pintos Build for " << paths.project << ": " << std::string{paths.build} << std::endl;
  }

  fs::current_path(paths.build);
  std::ofstream make_pincheck{"Make.pincheck"};
  if(!make_pincheck.is_open()){
    panic_msg << "Cannot make temp file to extract list of tests.";
    panic(panic_msg);
  }
  make_pincheck
    << "# -*- makefile -*-\n\n"
    << "SRCDIR = ../..\n\n"
    << "all:\n\t@echo $(TESTS) $(EXTRA_GRADES)\n\n"
    << "include ../../Make.config\n"
    << "include ../Make.vars\n"
    << "include ../../tests/Make.tests\n";
  make_pincheck.close();

  const auto make_pincheck_res = exec_str("make --silent -f Make.pincheck");
  if (make_pincheck_res.first != 0 || !make_pincheck_res.second.has_value()) {
    panic_msg << "Cannot extract list of tests.";
    panic(panic_msg);
  }
  const auto all_tests = string_tokenize(*make_pincheck_res.second);
  Vector<Pair<String>> target_tests{};
  const auto name_patterns = program.get<Vector<String>>("--");
  const auto subdir_patterns = program.get<Vector<String>>("--subdir");
  const auto name_ex_patterns = program.get<Vector<String>>("--exclude");
  const auto subdir_ex_patterns = program.get<Vector<String>>("--subdir-exclude");
  for(const auto& _test : all_tests) {
    const auto& test = string_trim(_test);

    const auto test_path = Path{test};
    auto name = String{test_path.filename()};
    auto subdir = String{test_path.parent_path()};

    if(!wildcard_match(subdir, subdir_patterns) ||
       !wildcard_match(name, name_patterns) ||
       wildcard_match(subdir, subdir_ex_patterns) ||
       wildcard_match(name, name_ex_patterns)) continue;

    target_tests.emplace_back(std::move(subdir), std::move(name));
  }

  const auto pool_size = program.get<unsigned>("-j");
  std::vector<TestResult> results, results_cache;
  std::vector<std::unique_ptr<TestRunner>> pool(pool_size);
  size_t next = 0;
  
  // init pool print
  std::cout << std::endl;
  const String omit_msg = " ... ";
  constexpr auto COL_JITTER = 3;
  while(results.size() < target_tests.size()) {
    for(size_t i = 0; i < pool_size; ++i) {
      if(pool[i]) {
        if(pool[i]->is_finished()) {
          results_cache.emplace_back(*pool[i]);
          pool[i] = nullptr;
        }
      }
    }
    for(size_t i = 0; i < pool_size && next < target_tests.size(); ++i) {
      if(pool[i]) continue;
      pool[i] = std::make_unique<TestRunner>(target_tests[next].first, target_tests[next].second);
      pool[i]->register_test(paths);
      ++next;
    }

    if(!results_cache.empty()) {
      std::cout << "\033[2K\033[1G";
      for(const auto& r : results_cache) {
        r.print_row();
        std::cout << std::endl;
        results.emplace_back(r);
      }
      results_cache.clear();
    }

    const auto running_pools = std::count_if(pool.cbegin(), pool.cend(),
      [](const std::unique_ptr<TestRunner>& p){return p!=nullptr;});
    const String full_pool_msg = "Running"s + "(" + std::to_string(running_pools) + "/" + std::to_string(pool_size) + ") : ";
    std::cout << "\033[2K\033[1G";
    std::cout << termcolor::reset << termcolor::bold << termcolor::yellow
      << full_pool_msg;
    std::cout << termcolor::reset << termcolor::yellow;
    std::string pool_str;
    for(auto& p : pool) {
      if(!p) continue;
      pool_str += p->get_print() + " ";
      if (const size_t ws_col = get_winsize().ws_col;
        pool_str.size() + full_pool_msg.size() + COL_JITTER >= ws_col) {
        pool_str = pool_str.substr(0, ws_col - full_pool_msg.size() - COL_JITTER - omit_msg.size());
        pool_str += omit_msg;
        break;
      }
    }
    std::cout << pool_str << termcolor::reset << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  unsigned passed = std::count_if(results.begin(), results.end(),
    [](const TestResult& r){return r.passed;});

  std::cout << "\033[2K\033[1G";
  std::cout << "\nFinished total " << termcolor::bold << target_tests.size() << " tests." << termcolor::reset << std::endl;
  std::cout << termcolor::green << "Pass: " << termcolor::bold << passed << '\t';
  std::cout << termcolor::reset << termcolor::red << "Fail: " << termcolor::bold << target_tests.size() - passed;
  std::cout << termcolor::reset << std::endl << std::endl;
  
  const int return_value = (passed == target_tests.size() ? 0 : 1);
  std::cout << "pincheck exiting with code " << return_value << std::endl;
  return return_value;
}
