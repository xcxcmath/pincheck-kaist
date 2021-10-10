#include <iostream>

#include "version_check.h"
#include "termcolor/termcolor.hpp"
#include "execution.h"
#include "string_helper.h"

static void print_new_version(const String &new_version) {
  std::cout << termcolor::bright_magenta << termcolor::bold;
  std::cout << "New version available : " << new_version;
  std::cout << termcolor::reset << std::endl;
  std::cout << "Current version : " << PINCHECK_VERSION << std::endl;
  std::cout << "To upgrade pincheck, go to the cloned directory and run:\n";
  std::cout << termcolor::bright_white << "\t$ git pull\n\t$ make install";
  std::cout << termcolor::reset << std::endl << std::endl;
}

static bool do_check_new_version(const String &github_fetch_cmd, bool is_verbose) {
  constexpr auto PY_CMD = "python3 -c \"import sys, json; print(json.load(sys.stdin)['tag_name'])\"";
  try {
    const auto cmd = github_fetch_cmd + " | " + PY_CMD;
    if (is_verbose)
      std::cout << "Version checking command : " << cmd << std::endl;
    const auto res = exec_str(cmd.c_str());
    if(!(res.first != 0 || !res.second)) {
      const auto new_version = string_trim(*res.second);
      if (new_version != PINCHECK_VERSION) {
        print_new_version(new_version);
        return true;
      } else {
        if (is_verbose)
          std::cout << "Same version found." << std::endl;
        return false;
      }
    }
  } catch (const std::exception &e) {
  }
  if (is_verbose)
    std::cout << "Failed to fetch and compare version." << std::endl;
  return false;
}

void check_new_version(bool is_verbose){
  using namespace std::string_literals;
  constexpr auto GITHUB_URL = "https://api.github.com/repos/xcxcmath/pincheck-kaist/releases/latest";
  
  try {
    if (is_verbose)
      std::cout << "Version checking...\n";

    // 1. curl
    const auto curl_cmd = "curl -s "s + GITHUB_URL + " 2> /dev/null";
    if(do_check_new_version(curl_cmd, is_verbose)) return;

    // 2. wget
    const auto wget_cmd = "wget -nv -O - "s + GITHUB_URL + " 2> /dev/null";
    if(do_check_new_version(wget_cmd, is_verbose)) return;
  } catch (const std::exception &e) {

  }
}