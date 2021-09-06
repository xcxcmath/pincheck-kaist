#include <iostream>
#include "execution.h"
#include "test_path.h"
#include "string_helper.h"

TestPath::TestPath() = default;

Path detect_src(TestPath &paths) {
  std::ostringstream panic_msg;

  const auto which_pintos = exec_str("which pintos");
  if(which_pintos.first != 0 || !which_pintos.second.has_value() || (*which_pintos.second).empty()) {
    panic("Cannot find pintos location with `which pintos` command.");
  }

  const String which_pintos_res = *which_pintos.second;
  Path p{string_trim(which_pintos_res)};

  if(!fs::exists(p) || !p.has_parent_path()) {
    panic_msg << "Cannot find pintos directory, as it seems to have invalid path: ";
    panic_msg << p;
    panic(panic_msg);
  }

  p = p.parent_path(); // now it should be 'utils'

  if(!fs::exists(p) || !p.has_parent_path() || p.filename() != "utils") {
    panic_msg << "Proper pintos executable must be inside `utils` with the parent directory...\n";
    panic_msg << "Detected dirname for pintos: " << p;
    panic(panic_msg);
  }

  return paths.src = fs::absolute(p.parent_path());
}

String detect_project(TestPath &paths) {
  std::ostringstream panic_msg, temp;
  auto p = fs::current_path();
  if (p.filename() == "build") {
    p = p.parent_path();
  }

  if(!fs::equivalent(p.parent_path(), paths.src)) {
    panic_msg << "The current path seems not to be inside pintos project directory.\n";
    panic_msg << "Please move path to any project directory or its build directory.";
    panic(panic_msg);
  }

  return paths.project = p.filename();
}

Path detect_build(TestPath &paths, bool verbose, bool clean) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;

  const auto make_dir = std::string{paths.src / paths.project};
  if(clean){
    if(verbose) {
      std::cout << "Cleaning build..." << std::flush;
    }
    const auto clean_cmd = "make clean -C "s + make_dir + " 2>&1"s;
    const auto clean_ret = exec_str(clean_cmd.c_str());
    if(!clean_ret.second.has_value()) {
      panic_msg << "FAILURE : cannot properly execute " << clean_cmd;
      panic(panic_msg);
    }
    if(clean_ret.first != 0) {
      panic_msg << "make command exited with nonzero code: " << clean_ret.first;
      panic_msg << std::endl << "See detailed output: " << *clean_ret.second;
      panic(panic_msg);
    }
    if(verbose) {
      std::cout << "finished" << std::endl;
    }
  }

  // make project
  const auto cmd = "make -j " + std::to_string(HARDWARE_CONCURRENCY) + " -C "s + make_dir + " 2>&1"s;
  if (verbose)
    std::cout << "Building '" << cmd << "'..." << std::flush;
  const auto make_ret = exec_str(cmd.c_str());
  if(!make_ret.second.has_value()) {
    panic_msg << "FAILURE : cannot properly execute " << cmd;
    panic(panic_msg);
  }
  if(make_ret.first != 0) {
    panic_msg << "make command exited with nonzero code: " << make_ret.first;
    panic_msg << std::endl << "See detailed output: " << *make_ret.second;
    panic(panic_msg);
  }
  paths.build = paths.src / paths.project / "build";
  if(!fs::exists(paths.build / "kernel.bin")) {
    panic_msg << "make command didn't make kernel.";
    panic(panic_msg);
  }

  if(verbose)
    std::cout << "finished" << std::endl;
  
  return paths.build;
}

Path make_pool(TestPath &paths, size_t size) {
  std::ostringstream panic_msg;
  try {
  paths.pools_root = paths.build / "pools";
  paths.pool = paths.pools_root / paths.project;

  fs::create_directories(paths.pools_root);
  fs::create_directories(paths.pool);

  paths.pool_instances = std::vector<fs::path>();
  for(size_t i = 0; i < size; ++i) {
    paths.pool_instances.emplace_back(paths.pool / (std::string{"build"} + std::to_string(i)));
    fs::create_directories(paths.pool_instances[i]);
  }

  // copy first one
  std::vector<fs::path> build_results = {
    paths.build/"os.dsk",
    paths.build/"tests",
  };
  /*std::vector<fs::path> build_results;
  for(const auto& dir_entry: fs::directory_iterator{paths.build}) {
    if(!fs::equivalent(dir_entry, paths.pools_root)){
      build_results.emplace_back(dir_entry);
    }
  }*/

  const auto cp_op = fs::copy_options::recursive | fs::copy_options::update_existing;
  for(const auto& entry: build_results) {
    if(fs::is_directory(entry)) {
      fs::copy(entry, paths.pool_instances[0] / entry.filename(), cp_op);
    } else if(fs::is_regular_file(entry)) {
      fs::copy(entry, paths.pool_instances[0], cp_op);
    }
  }

  // copy to the others
  for(size_t i = 1; i < size; ++i) {
    fs::copy(paths.pool_instances[0], paths.pool_instances[i], cp_op);
  }

  return paths.pool;
  } catch (const fs::filesystem_error& fe) {
    panic_msg << "File system error occured during making test pool:" << std::endl;
    panic_msg << "\t" << fe.what();
    panic(panic_msg);
  }
}
