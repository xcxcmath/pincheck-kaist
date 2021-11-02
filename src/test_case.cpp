#include "test_case.h"

TestCase::TestCase(String subdir, String name)
: name(std::move(name)), subdir(std::move(subdir))
, timeout(0)
, subtitle()
, max_ptr()
, persistence(false) {}

String TestCase::full_name() const {
  return subdir + "/" + name;
}

bool TestCase::operator<(const TestCase &rhs) const {
  return timeout < rhs.timeout;
}