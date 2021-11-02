#ifndef PINCHECK_TEST_RESULT_H
#define PINCHECK_TEST_RESULT_H

#include <deque>
#include <atomic>
#include <chrono>
#include "common.h"
#include "test_case.h"

class TestResult {
  public:
    TestCase testcase;
    bool passed;
    int exit_code;
    String dump;
    const char* except_dump;
    std::chrono::system_clock::time_point start_time, end_time;

    TestResult(const TestCase&, bool passed, int exit_code, const String& dump, const char* except_dump,
      std::chrono::system_clock::time_point start_time,
      std::chrono::system_clock::time_point end_time);
    void print_row(bool detail, bool verbose) const;
};


#endif
