#include <iostream>
#include <array>
#include <stdexcept>
#include <sstream>

#include <thread>

#include <unistd.h>
#include <sys/wait.h>

#include "termcolor/termcolor.hpp"
#include "execution.h"

Pair<int, Optional<String>> exec_str(const char *cmd) noexcept {
  FILE *p = nullptr;
  String out;
  try {
    Buffer buffer;
    std::ostringstream os;
    p = popen(cmd, "r");
    if (!p) {
      return {-1, std::nullopt};
    }
    while(fgets(buffer.data(), buffer.size(), p) != nullptr) {
      os << buffer.data();
    }

    out = os.str();
  } catch(const std::exception& e){
    if(p) {
      pclose(p);
    }

    return {-1, std::nullopt};
  }

  int wret = pclose(p);
  if(WIFEXITED(wret)) {
    return {WEXITSTATUS(wret), out};
  }

  return {-1, std::nullopt};
}

int exec_ret(const char *cmd) noexcept {
  Buffer buffer;
  FILE *p = popen(cmd, "r");
  if (!p) {
    return -1;
  }

  try {
    while(fgets(buffer.data(), buffer.size(), p) != nullptr);
  } catch (const std::exception& e) {
    pclose(p);
    return -1;
  }

  int wret = pclose(p);
  if(WIFEXITED(wret)) {
    return WEXITSTATUS(wret);
  }
  return -1;
}


const unsigned HARDWARE_CONCURRENCY = std::thread::hardware_concurrency();


[[noreturn]] void panic(const std::string& msg, int exit_code) {
  std::cerr << termcolor::red << std::endl << msg << std::endl;
  std::cerr << std::endl << "pincheck exiting with panic.." << std::endl << termcolor::reset;
  std::exit(exit_code);
}
[[noreturn]] void panic(const std::ostringstream& os, int exit_code) {
  panic(os.str(), exit_code);
}