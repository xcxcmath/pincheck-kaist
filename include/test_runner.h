#ifndef PINCHECK_TEST_RUNNER_H
#define PINCHECK_TEST_RUNNER_H

#include <mutex>
#include <future>
#include <chrono>
#include "test_case.h"
#include "test_path.h"
#include "common.h"

class TestResult;
class TestRunner {
private:
  TestCase testcase;
  bool running, finished, passed;
  int exit_code;
  String dump;
  const char* except_dump;

  std::chrono::system_clock::time_point start_time, end_time;
  std::future<void> fut;
  std::mutex mut;

  TestRunner(const TestRunner&) = delete;
  TestRunner& operator=(const TestRunner&) = delete;
  TestRunner(TestRunner&&) = delete;
  TestRunner& operator=(TestRunner&&) = delete;

public:
  friend TestResult;
  TestRunner(TestCase testcase);
  ~TestRunner() noexcept;

  const TestCase& get_test_case() const;
  
  bool is_running();
  bool is_finished();
  int get_exit_code();
  String get_dump();
  const char *get_except_dump();

  void register_test(const TestPath& paths) noexcept;
  String get_print();
};

#endif