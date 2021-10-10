#ifndef PINCHECK_CHECK_RUNNER_H
#define PINCHECK_CHECK_RUNNER_H

#include "common.h"
#include "test_path.h"
#include "test_case.h"

int check_run(const TestPath &paths, const Vector<TestCase> &target_tests,
  bool is_verbose, unsigned pool_size, unsigned repeats);

#endif
