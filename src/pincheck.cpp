#include <fstream>
#include <memory>
#include <set>
#include <unordered_set>
#include <iterator>
#include <regex>

#include <sys/ioctl.h>

#include "termcolor/termcolor.hpp"
#include "argparse/argparse.hpp"
#include "execution.h"
#include "test_path.h"
#include "test_runner.h"
#include "test_result.h"
#include "gdb_runner.h"
#include "string_helper.h"

struct TestCase {
  String name, subdir;
  int timeout;
  TestCase(String subdir, String name):name(std::move(name)), subdir(std::move(subdir)), timeout(0){}

  String full_name() const {
    return subdir + "/" + name;
  }

  bool operator<(const TestCase &rhs) const {
    return timeout < rhs.timeout;
  }
};

struct winsize get_winsize() {
  struct winsize winsize;
  ioctl(0, TIOCGWINSZ, &winsize);
  return winsize;
}

enum class PincheckMode {
  check, run, gdb
};

static int parse_timeout(const String &command);
static String get_running_command(const String &full_name, bool gdb_opt, bool timeout_opt);
static int run_mode_check (argparse::ArgumentParser &program, const TestPath &paths, const Vector<TestCase> &target_tests);
static int run_mode_run (argparse::ArgumentParser &program, const Vector<TestCase> &target_tests);
static int run_mode_gdb (argparse::ArgumentParser &program, const Vector<TestCase> &target_tests);

int main(int argc, char *argv[]) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;
  auto mode = PincheckMode::check;

  const std::chrono::system_clock::time_point pincheck_start = std::chrono::system_clock::now();

  std::cout << termcolor::reset;
  std::cerr << termcolor::reset;

  argparse::ArgumentParser program("pincheck", "v21.09.23");
  program.add_argument("-p", "--project")
         .help("Pintos project to run test; threads, userprog, vm, or filesys");
  program.add_argument("-j", "--jobs")
         .help("Maximum number of parallel test execution")
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
  program.add_argument("-e", "--exclude", "--except")
         .help("Test name to be excluded; can be given multiple times")
         .default_value(Vector<String>{})
         .append();
  program.add_argument("-sde", "--subdir-exclude", "--subdir-except")
         .help("Test subdir to be excluded; can be given multiple times")
         .default_value(Vector<String>{})
         .append();
  program.add_argument("-S", "--sort")
         .help("Sort test cases first in decreasing order of TIMEOUT, which may help to check all faster")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-jr", "--just-run")
         .help("Run a case getting the output; only one at a time is required");
  program.add_argument("-gr", "--gdb-run")
         .help("Run a case getting the output with embedded GDB REPL; only one at a time is required");
  program.add_argument("-r", "--repeat")
         .help("# of repeating the whole checking")
         .scan<'i', unsigned>()
         .default_value(static_cast<unsigned>(1));
  program.add_argument("--gdb")
         .help("Run with --gdb option for pintos; used with --just-run")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("--with-timeout")
         .help("Run with TIMEOUT (-T option) for pintos; used with --just-run")
         .default_value(false)
         .implicit_value(true);
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
  Vector<TestCase> target_tests{};
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

    TestCase here(std::move(subdir), std::move(name));

    if(program.get<bool>("--sort")) {
      const auto cmd = get_running_command(here.full_name(), false, true);
      here.timeout = parse_timeout(cmd);
    }

    target_tests.push_back(here);
  }
  if(program.get<bool>("--sort")) {
    std::sort(target_tests.rbegin(), target_tests.rend());
  }
  std::cout << termcolor::bold << "Total " << target_tests.size() << " tests found." << termcolor::reset << std::endl;
  if (is_verbose) {
    std::cout << "-- Target tests --" << std::endl;
    for(const auto& test_case : target_tests) {
      std::cout << test_case.subdir << " -> " << test_case.name << std::endl;
    }
  }


  //--------------------------------------------------------

  if(program.is_used("--just-run")) {
    mode = PincheckMode::run;
  } else if(program.is_used("--gdb-run")) {
    mode = PincheckMode::gdb;
  }

  //--------------------------------------------------------

  int exit_code = 1;
  switch(mode){
    case PincheckMode::run:
      exit_code = run_mode_run (program, target_tests);
      break;
    
    case PincheckMode::gdb:
      exit_code = run_mode_gdb (program, target_tests);
      break;
    
    case PincheckMode::check:
      exit_code = run_mode_check (program, paths, target_tests);
      break;
    
    default:
      panic("Unsupported running mode");
  }

  const std::chrono::system_clock::time_point pincheck_end = std::chrono::system_clock::now();
  std::cout << "pincheck exiting with code " << exit_code
    << " (Running time: " << std::chrono::duration_cast<std::chrono::seconds>(pincheck_end-pincheck_start).count() << " sec)" << std::endl;
  return exit_code;
}

/** Implementation parts */

static int run_mode_check (argparse::ArgumentParser &program, const TestPath &paths, const Vector<TestCase> &target_tests) {
  using namespace std::string_literals;

  const auto is_verbose = program.get<bool>("--verbose");
  const auto pool_size = program.get<unsigned>("-j");

  const auto repeats = program.get<unsigned>("--repeat");

  const String omit_msg = " ... ";
  constexpr auto COL_JITTER = 3;

  unsigned epoch_passed = 0;

  for(unsigned epoch = 1; epoch <= repeats; ++epoch){
  Vector<TestResult> results, results_cache;
  Vector<std::unique_ptr<TestRunner>> pool(pool_size);

  size_t next = 0;

  if(repeats > 1) {
    std::cout << termcolor::bold << "\nEpoch " << epoch << " of " << repeats << termcolor::reset;
    std::cout << " (so far: " << termcolor::green << epoch_passed << " epochs passed, "
      << termcolor::red << (epoch - epoch_passed - 1) << " epochs failed" << termcolor::reset << ")" << std::endl;
  }
  
  // init pool print
  std::cout << std::endl;
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
      pool[i] = std::make_unique<TestRunner>(target_tests[next].subdir, target_tests[next].name);
      pool[i]->register_test(paths);
      ++next;
    }

    if(!results_cache.empty()) {
      std::cout << "\033[2K\033[1G";
      for(const auto& r : results_cache) {
        r.print_row(true);
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
  unsigned failed = target_tests.size() - passed;

  std::cout << "\033[2K\033[1G";
  std::cout << "\nFinished total " << termcolor::bold << target_tests.size() << " tests." << termcolor::reset << std::endl;
  
  const bool all_passed = (passed == target_tests.size());

  if (!all_passed) {
    std::cout << "\n" << termcolor::bright_red << "-- Failed tests --" << termcolor::reset << std::endl;
    for (const auto& tr : results) {
      if(!tr.passed) {
        tr.print_row(is_verbose);
        std::cout << std::endl;
      }
    }
    std::cout << std::endl;
  }
  std::cout << termcolor::green << "Pass: ";
  if(passed != 0) std::cout << termcolor::bold;
  std::cout << passed << '\t';

  std::cout << termcolor::reset << termcolor::red << "Fail: ";
  if(failed != 0) std::cout << termcolor::bold;
  std::cout << failed;
  std::cout << termcolor::reset << std::endl << std::endl;

  if (all_passed) {
    std::cout << termcolor::blue << termcolor::bold << "Correct!" << termcolor::reset << std::endl;
    epoch_passed++;
  }

  } // for-loop of epoch

  bool all_epoch_passed = epoch_passed == repeats;
  int return_value = all_epoch_passed ? 0 : 1;
  if (repeats > 1) {
    unsigned epoch_failed = repeats - epoch_passed;
    std::cout << std::endl;
    std::cout << termcolor::bold << "All " << repeats << " trials done." << std::endl;
    std::cout << termcolor::reset << termcolor::green << "All passing epochs: ";
    if(epoch_passed != 0) std::cout << termcolor::bold;
    std::cout << epoch_passed << std::endl;

    std::cout << termcolor::reset << termcolor::red << "Failed epochs: ";
    if(epoch_failed != 0) std::cout << termcolor::bold;
    std::cout << epoch_failed;
    std::cout << termcolor::reset << std::endl << std::endl;

    if (all_epoch_passed) {
      std::cout << termcolor::blue << termcolor::bold << "Succeeded for all trials!" << termcolor::reset << std::endl;
    }
  }

  return return_value;
}

static auto get_target_test_or_panic(const String &t, const Vector<TestCase> &target_tests) {
  std::ostringstream panic_msg;
  auto it = std::find_if(target_tests.cbegin(), target_tests.cend(), [&t](const TestCase &here){
    return t == here.name || t == here.full_name();
  });
  if(it == target_tests.cend()) {
    panic_msg << "Cannot find the case named " << t;
    panic(panic_msg);
  }

  return it;
}

static int parse_timeout(const String &command) {
  constexpr int DEFAULT_TIMEOUT = 60;
  const auto tokens = string_tokenize(command);

  auto it = std::find(tokens.cbegin(), tokens.cend(), "-T");
  if(it == tokens.cend()) {
    return DEFAULT_TIMEOUT;
  }

  ++it;
  if(it == tokens.cend()) {
    return DEFAULT_TIMEOUT;
  }

  int ret;
  try {
    ret = std::stoi(*it);
  } catch (const std::exception &e) {
    ret = DEFAULT_TIMEOUT;
  }

  return ret;
}

static String get_running_command(const String &full_name, bool gdb_opt, bool timeout_opt) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;
  const auto get_run_cmd_command = "make "s + full_name
    + ".output --dry-run --silent --assume-old=os.dsk --what-if=os.dsk";

  const auto get_run_cmd_result = exec_str(get_run_cmd_command.c_str());
  if (get_run_cmd_result.first != 0 || !get_run_cmd_result.second) {
    panic_msg << "Cannot find out the command line to run the case";
    panic(panic_msg);
  }

  auto full_run_command = string_trim(*(get_run_cmd_result.second));
  const auto cut_idx = full_run_command.find('<');
  if(cut_idx == String::npos) {
    panic_msg << "The command line to run the case seems to be not valid:\n";
    panic_msg << full_run_command;
    panic(panic_msg);
  }

  full_run_command = string_trim(full_run_command.substr(0, cut_idx));

  if(gdb_opt) {
    const auto gdb_idx = full_run_command.find(' ');
    if (gdb_idx == String::npos) {
      full_run_command += " --gdb";
    } else {
      full_run_command = full_run_command.substr(0, gdb_idx) + " --gdb" + full_run_command.substr(gdb_idx);
    }
  }

  if(!timeout_opt) {
    const auto t_opt = "-T"s;
    const auto t_idx = full_run_command.find(t_opt);
    if (t_idx != String::npos) {
      size_t i = t_idx + t_opt.size();
      for(;i < full_run_command.size(); ++i) {
        if(!std::isspace(full_run_command[i])) {
          break;
        }
      }
      if(i != full_run_command.size()) {
        i = i+1;
        for(;i < full_run_command.size(); ++i) {
          if(std::isspace(full_run_command[i])) {
            break;
          }
        }

        if(i == full_run_command.size()) {
          full_run_command = full_run_command.substr(0, t_idx);
        } else {
          full_run_command = full_run_command.substr(0, t_idx) + full_run_command.substr(i);
        }
      }
    }
  }

  return full_run_command;
}

static int run_mode_run (argparse::ArgumentParser &program, const Vector<TestCase> &target_tests) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;

  const auto is_verbose = program.get<bool>("--verbose");
  const auto target_test = program.get<String>("--just-run");
  auto it = get_target_test_or_panic(target_test, target_tests);

  const auto full_name = it->full_name();
  std::cout << "Detected just-running mode for "
    << termcolor::bold << termcolor::blue << full_name << termcolor::reset << std::endl << std::endl;

  const auto run_command = get_running_command(full_name, program.get<bool>("--gdb"), program.get<bool>("--with-timeout"))
    + " | tee " + full_name + ".output";
  if (is_verbose)
    std::cout << "Running command: " << run_command << std::endl << std::endl;;
  
  const auto ret = system(run_command.c_str());
  std::cout << std::endl;
  if (ret != 0) {
    panic_msg << "The shell command line execution returned with non-zero: " << ret;
    panic(panic_msg);
  }

  // PANIC detection
  int return_value = 0;
  
  std::ifstream output_fs(full_name + ".output");
  if(!output_fs.is_open()) {
    panic_msg << "Cannot open the output file; cannot detect whether PANIC happened";
    panic(panic_msg);
  }

  String line;
  bool panic_detected = false, call_stack_detected = false;
  std::smatch panic_match;
  std::regex panic_re("PANIC", std::regex::grep);
  String panic_line;
  std::smatch call_stack_match;
  std::regex call_stack_re("Call stack\\:");
  String call_stack_line;
  while(std::getline(output_fs, line)) {
    if(std::regex_search(line, panic_match, panic_re)) {
      panic_detected = true;
      panic_line = line;
    } else if (std::regex_search(line, call_stack_match, call_stack_re)) {
      call_stack_detected = true;
      call_stack_line = call_stack_match.suffix();
    }
  }

  if(!panic_detected) {
    std::cout << termcolor::green << termcolor::bold;
    std::cout << "No PANIC message detected" << std::endl;
  } else {
    std::cout << termcolor::yellow << termcolor::bold;
    std::cout << "-- PANIC detected on the pintos execution --" << std::endl;
    std::cout << termcolor::reset << termcolor::yellow << panic_line << std::endl;

    if (!call_stack_detected) {
      panic_msg << "PANIC detected, but no call stack found";
      panic(panic_msg);
    }
    
    std::regex re("(0x[0-9a-f]+)+");
    std::ostringstream backtrace_cmd_os;
    backtrace_cmd_os << "backtrace ";
    const auto call_stack_tokens = string_tokenize(call_stack_line);
    for(const auto &s: call_stack_tokens) {
      if(std::regex_match(s, re)) {
        backtrace_cmd_os << s << " ";
      } else break;
    }
    const auto backtrace_cmd = backtrace_cmd_os.str();
    std::cout << "Backtrace command: " << backtrace_cmd << std::endl;

    const auto backtrace_res = exec_str(backtrace_cmd.c_str());
    if(backtrace_res.first != 0 || !backtrace_res.second) {
      panic_msg << "Backtrace command failed...";
      panic(panic_msg);
    }

    std::cout << "Here are the backtrace results:" << std::endl;
    std::cout << *(backtrace_res.second) << std::endl;
    return_value = 1;
  }
  std::cout << termcolor::reset << std::endl;
  output_fs.close();

  return return_value;
}

static int run_mode_gdb (argparse::ArgumentParser &program, const Vector<TestCase> &target_tests){
  using namespace std::string_literals;
  std::ostringstream panic_msg;

  const auto is_verbose = program.get<bool>("--verbose");
  const auto target_test = program.get<String>("--gdb-run");
  auto it = get_target_test_or_panic(target_test, target_tests);

  const auto full_name = it->full_name();
  std::cout << "Detected gdb-running mode for "
    << termcolor::bold << termcolor::magenta << full_name << termcolor::reset << std::endl << std::endl;

  const auto server_run_command = get_running_command(full_name, true, false) + " < /dev/null 2>&1";
  if (is_verbose)
    std::cout << "GDB server running command: " << server_run_command << std::endl;

  return gdb_run(server_run_command);
}
