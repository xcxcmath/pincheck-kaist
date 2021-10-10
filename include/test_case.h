#ifndef PINCHECK_TEST_CASE_H
#define PINCHECK_TEST_CASE_H

#include "common.h"

struct TestCase {
  String name, subdir;
  int timeout;

  String subtitle;
  unsigned max_ptr;

  TestCase(String subdir, String name);
  String full_name() const;

  bool operator<(const TestCase &rhs) const;
};

#endif