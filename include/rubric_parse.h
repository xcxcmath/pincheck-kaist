#ifndef PINCHECK_RUBRIC_PARSE_H
#define PINCHECK_RUBRIC_PARSE_H

#include <unordered_map>
#include "common.h"
#include "test_case.h"

struct Rubric {
  String subdir, subdir_suffix;
  String title;
  double max_pct;
  std::unordered_map<String, Vector<String>> subtitles;
};

Vector<Rubric> parse_rubric(const Path& grading_file, Vector<TestCase> &target_tests, Vector<TestCase> &persistence_tests);

#endif