#ifndef PINCHECK_TEST_PATH_H
#define PINCHECK_TEST_PATH_H

#include <unordered_set>

#include "common.h"

struct TestPath {
  Path src, build, pool, pools_root;
  String project;
  Vector<Path> pool_instances;
  TestPath();
};

Path detect_src(TestPath &paths);
String detect_project(TestPath &paths);
Path detect_build(TestPath &paths, bool verbose, bool clean);
Path make_pool(TestPath &paths, size_t size);

#endif