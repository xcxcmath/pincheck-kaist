#include <fstream>
#include <map>
#include <random>

#include "termcolor/termcolor.hpp"

#include "arg_parse.h"
#include "version_check.h"
#include "test_case.h"
#include "execution.h"
#include "test_path.h"
#include "rubric_parse.h"

#include "test_runner.h"
#include "test_result.h"

#include "check_runner.h"
#include "just_runner.h"
#include "gdb_runner.h"

#include "string_helper.h"
#include "console_helper.h"

const char *PINCHECK_VERSION = "v21.11.09";

enum class PincheckMode {
  check, run, gdb
};

struct CacheEntry {
  bool persistence;
  int timeout;
};

static int parse_timeout(const String &command);
static Optional<String> get_raw_running_command(const String &full_name);
static String get_running_command(const String &full_name, bool gdb_opt, bool timeout_opt);

static int run_mode_check (argparse::ArgumentParser &program, const TestPath &paths, const Vector<TestCase> &target_tests, const Vector<TestCase> &persistence_tests);
static int run_mode_run (argparse::ArgumentParser &program, const Vector<TestCase> &target_tests);
static int run_mode_gdb (argparse::ArgumentParser &program, const Vector<TestCase> &target_tests);

int main(int argc, char *argv[]) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;
  auto mode = PincheckMode::check;

  const std::chrono::system_clock::time_point pincheck_start = std::chrono::system_clock::now();

  std::cout << termcolor::reset;
  std::cerr << termcolor::reset;

  argparse::ArgumentParser program("pincheck", PINCHECK_VERSION);
  parse_args(program, argc, argv);
  const auto is_verbose = program.get<bool>("--verbose");

  std::random_device rd;
  std::mt19937 gen(rd());
  if (std::bernoulli_distribution d(1.0/4);
    (program.get<bool>("-fv") || d(gen)) && !program.get<bool>("-nv")) {
    check_new_version(is_verbose);
  }

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
    << ".PHONY: tests grade_file\n\n"
    << "tests:\n\t@echo $(TESTS) $(EXTRA_GRADES) "
    << "$(foreach subdir,$(TEST_SUBDIRS),$($(subdir)_GRADES))\n\n"
    << "grade_file:\n\t@echo $(GRADING_FILE)\n\n"
    << "include ../../Make.config\n"
    << "include ../Make.vars\n"
    << "include ../../tests/Make.tests\n";
  make_pincheck.close();

  const auto make_tests_res = exec_str("make tests --silent -f Make.pincheck");
  if (make_tests_res.first != 0 || !make_tests_res.second.has_value()) {
    panic_msg << "Cannot extract list of tests.";
    panic(panic_msg);
  }
  const auto all_tests = string_tokenize(*make_tests_res.second);

  // pincheck cache
  std::ifstream cache_file_input{"cache.pincheck"};
  std::unordered_map<String, CacheEntry> cache_map;
  if(cache_file_input.is_open()) {
    String line;
    while(std::getline(cache_file_input, line)) {
      auto tokens = string_tokenize(line);
      if(tokens.size() != 3) {
        continue;
      }

      bool persistence;
      int timeout;
      try {
        persistence = (std::stoi(tokens[1]) != 0);
        timeout = std::stoi(tokens[2]);
      } catch (std::exception&) {
        continue;
      }
      cache_map[tokens[0]] = CacheEntry{.persistence = persistence, .timeout = timeout};
    }
    cache_file_input.close();
  }

  std::cout << "Extracting list of tests. May take some times..." << std::endl;
  Vector<TestCase> target_tests{};
  Vector<TestCase> persistence_tests{};
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

    auto cache_it = cache_map.find(here.full_name());
    bool add_to_cache = false;
    if(cache_it != cache_map.end()) {
      here.timeout = cache_it->second.timeout;
      here.persistence = cache_it->second.persistence;
    } else {
      const auto opt_cmd = get_raw_running_command(here.full_name());
      if(!opt_cmd) {
        auto pers_it = std::find(all_tests.cbegin(), all_tests.cend(), here.full_name() + "-persistence");
        if(pers_it != all_tests.cend()) {
          here.persistence = true;
          here.timeout = 60;
        } else {
          continue;
        }
      } else {
        here.timeout = parse_timeout(*opt_cmd);
      }

      add_to_cache = true;
    }

    if (here.persistence) {
      persistence_tests.push_back(here);
    } else {
      target_tests.push_back(here);
    }
    if(add_to_cache) {
      CacheEntry entry;
      entry.persistence = here.persistence;
      entry.timeout = here.timeout;
      cache_map[here.full_name()] = entry;
    }
  }
  std::ofstream cache_file_output{"cache.pincheck"};
  if(cache_file_output.is_open()) {
    std::cout << "cache storing..\n";
    for(const auto &[s, e]: cache_map) {
      cache_file_output << s << ' ' << static_cast<int>(e.persistence) << ' ' << e.timeout << '\n';
    }
    cache_file_output.close();
  }
  if(program.get<bool>("--sort")) {
    std::sort(target_tests.rbegin(), target_tests.rend());
  }

  const auto full_test_size = target_tests.size() + 2 * persistence_tests.size();
  std::cout << std::endl;
  std::cout << termcolor::bold << "Total " << full_test_size << " tests found." << termcolor::reset << std::endl;

  const auto grade_file_res = exec_str("make grade_file --silent -f Make.pincheck");
  if (grade_file_res.first != 0 || !grade_file_res.second.has_value()) {
    panic_msg << "Cannot extract the name of grading file.";
    panic(panic_msg);
  }
  auto rubrics = parse_rubric(string_trim(*grade_file_res.second), target_tests, persistence_tests);

  if (is_verbose) {
    std::cout << "-- Target tests --" << std::endl;
    for(const auto& test_case : target_tests) {
      std::cout << test_case.full_name()
        << " " << termcolor::grey
        << "(" << test_case.max_ptr << " ptr)" << termcolor::reset << std::endl;
    }
    for(const auto& test_case : persistence_tests) {
      std::cout << test_case.full_name()
        << " " << termcolor::grey
        << "(" << test_case.max_ptr << " ptr)" << termcolor::reset << std::endl;
      std::cout << test_case.full_name() << "-persistence" << termcolor::reset << std::endl;
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
      exit_code = run_mode_check (program, paths, target_tests, persistence_tests);
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

static int run_mode_check (argparse::ArgumentParser &program, const TestPath &paths, const Vector<TestCase> &target_tests, const Vector<TestCase> &persistence_tests) {
  const auto is_verbose = program.get<bool>("--verbose");
  const auto pool_size = program.get<unsigned>("-j");

  const auto repeats = program.get<unsigned>("--repeat");
  return check_run(paths, target_tests, persistence_tests, is_verbose, pool_size, repeats);
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

static Optional<String> get_raw_running_command(const String &full_name) {
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
  if(std::count(full_run_command.begin(), full_run_command.end(), '\n') >= 2) {
    return std::nullopt;
  }

  return full_run_command;
}

static String get_running_command(const String &full_name, bool gdb_opt, bool timeout_opt) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;

  auto optional_command = get_raw_running_command(full_name);
  if(!optional_command) {
    panic_msg << "Cannot find out the command line to run the case";
    panic(panic_msg);
  }

  String full_run_command = *optional_command;

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
    << termcolor::bold << termcolor::blue << full_name << termcolor::reset << std::endl;
  if(!it->subtitle.empty())
    std::cout << "Subtitle : " << termcolor::magenta << it->subtitle << termcolor::reset << std::endl;
  std::cout << std::endl;

  const auto run_command = get_running_command(full_name, program.get<bool>("--gdb"), program.get<bool>("--with-timeout"))
    + " | tee " + full_name + ".output";
  if (is_verbose)
    std::cout << "Running command: " << run_command << std::endl << std::endl;;
  
  return just_run(run_command, full_name);
}

static int run_mode_gdb (argparse::ArgumentParser &program, const Vector<TestCase> &target_tests){
  using namespace std::string_literals;
  std::ostringstream panic_msg;

  const auto is_verbose = program.get<bool>("--verbose");
  const auto target_test = program.get<String>("--gdb-run");
  auto it = get_target_test_or_panic(target_test, target_tests);

  const auto full_name = it->full_name();
  std::cout << "Detected gdb-running mode for "
    << termcolor::bold << termcolor::blue << full_name << termcolor::reset << std::endl;
  if(!it->subtitle.empty())
    std::cout << "Subtitle : " << termcolor::magenta << it->subtitle << termcolor::reset << std::endl;
  std::cout << std::endl;

  const auto server_run_command = get_running_command(full_name, true, false) + " < /dev/null 2>&1";
  if (is_verbose)
    std::cout << "GDB server running command: " << server_run_command << std::endl;

  return gdb_run(server_run_command);
}
