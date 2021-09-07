#ifndef PINCHECK_TEST_RESULT_H
#define PINCHECK_TEST_RESULT_H

#include <deque>
#include <atomic>
#include <chrono>
#include "common.h"
#include "test_runner.h"

class TestResult {
  public:
    String subdir, name, full_name;
    bool passed;
    int exit_code;
    String dump;
    const char* except_dump;
    std::chrono::system_clock::time_point start_time, end_time;

    TestResult();
    TestResult(const TestRunner&);
    void print_row(bool detail) const;
};


#endif
