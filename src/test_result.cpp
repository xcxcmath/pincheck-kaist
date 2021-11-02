#include <iostream>
#include <fstream>
#include "execution.h"
#include "test_result.h"
#include "termcolor/termcolor.hpp"

TestResult::TestResult
  (const TestCase &testcase, bool passed, int exit_code, const String& dump, const char* except_dump,
    std::chrono::system_clock::time_point start_time,
    std::chrono::system_clock::time_point end_time)
: testcase(testcase)
, passed(passed), exit_code(exit_code)
, dump(dump), except_dump(except_dump)
, start_time(start_time), end_time(end_time){
}

void TestResult::print_row(bool detail, bool verbose) const {
  std::cout << termcolor::reset << termcolor::bold;
  if(passed) std::cout << termcolor::green << "pass  ";
  else if(except_dump || exit_code) std::cout << termcolor::bright_red << "ERROR ";
  else std::cout << termcolor::red << "FAIL  ";
  std::cout << termcolor::reset;

  std::cout << testcase.full_name() << termcolor::bright_grey;
  std::cout << " by "
    << (std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time)).count()
    << " sec" << termcolor::reset;
  if((!passed || verbose) && !testcase.subtitle.empty()) {
    std::cout << termcolor::magenta << " [" << testcase.subtitle << "]" << termcolor::reset;
  }
  if(!passed && detail) {
    std::cout << "\ncode : " << exit_code;
    std::cout << "\ndump : " << dump;
    if(except_dump)
      std::cout << "\nexcept_dump : " << except_dump;
    std::cout << std::flush;
  }
  std::cout << termcolor::reset;
}
