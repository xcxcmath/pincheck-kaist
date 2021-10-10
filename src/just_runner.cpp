#include <iostream>
#include <fstream>
#include <regex>

#include "just_runner.h"
#include "execution.h"
#include "string_helper.h"
#include <termcolor/termcolor.hpp>

int just_run(const String &run_command, const String &full_name) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;

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