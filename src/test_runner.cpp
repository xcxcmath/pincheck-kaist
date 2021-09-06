#include <iostream>
#include <sstream>
#include <fstream>
#include "execution.h"
#include "test_runner.h"

TestRunner::TestRunner(std::string subdir, std::string name)
: subdir(std::move(subdir)), name(std::move(name))
, running(false), finished(false), passed(false)
, exit_code(0), dump(), except_dump(nullptr)
, start_time{}, end_time{}, fut{}, mut{}
{
  full_name = this->subdir + "/" + this->name;
}

TestRunner::~TestRunner() noexcept {
  if(fut.valid()) {
    try {
      fut.get();
    } catch (const std::exception& e) {
      except_dump = e.what();
    }
  }
}

std::string TestRunner::get_full_name() const {
  return full_name;
}

std::string TestRunner::get_subdir() const {
  return subdir;
}

std::string TestRunner::get_name() const {
  return name;
}

bool TestRunner::is_running() {
  std::unique_lock lock{mut};
  return running;
}

bool TestRunner::is_finished() {
  std::unique_lock lock{mut};
  return finished;
}

int TestRunner::get_exit_code() {
  std::unique_lock lock{mut};
  return exit_code;
}

std::string TestRunner::get_dump() {
  std::unique_lock lock{mut};
  return dump;
}

const char* TestRunner::get_except_dump() {
  std::unique_lock lock{mut};
  return except_dump;
}

void TestRunner::register_test(const TestPath& paths) noexcept {
  using namespace std::string_literals;

  try {
    fut = std::async(std::launch::async, [this, &paths]() {
      {
        std::unique_lock lock{mut};
        start_time = std::chrono::system_clock::now();
        running = true;
      }
      try {
        const auto result_file = full_name + ".result";
        const auto result_cmd = "make "s + String{result_file}
          + " --silent --assume-old=os.dsk --what-if=os.dsk 2>&1";
        const auto result_cmd_res = exec_str(result_cmd.c_str());
        if(result_cmd_res.first != 0 || !result_cmd_res.second) {
          {
            std::unique_lock lock{mut};
            dump = "Cannot run making result file properly";
            end_time = std::chrono::system_clock::now();
            finished = true;
            exit_code = result_cmd_res.first;
          }
          return;
        }

        std::ifstream res_fs(result_file);
        if(!res_fs.is_open()) {
          {
            std::unique_lock lock{mut};
            dump = "Cannot open result file";
            end_time = std::chrono::system_clock::now();
            finished = true;
            exit_code = -1;
          }
          return;
        }

        std::ostringstream os;
        std::string line;
        bool first_pass = false;
        bool first_line = true;
        while(std::getline(res_fs, line)) {
          if(first_line) {
            first_line = false;
            if(line == "PASS") {
              first_pass = true;
            }
          }
          os << line << '\n';
        }
        
        {
          std::unique_lock lock{mut};
          finished = true;
          end_time = std::chrono::system_clock::now();
          dump = os.str();
          passed = first_pass;
          exit_code = 0;
        }
        // get make .output cmd
        /*const auto output_path = full_name + ".output";
        const auto output_get_cmd = "make "s + std::string{output_path}
          + " --dry-run --silent --assume-old=os.dsk --what-if=os.dsk";*/
      } catch (const std::exception& e) {
        except_dump = e.what();
        end_time = std::chrono::system_clock::now();
        finished = true;
      }
    });
  } catch (const std::exception& e) {
    except_dump = e.what();
    end_time = std::chrono::system_clock::now();
    finished = true;
  }
}

String TestRunner::get_print() {
  std::unique_lock lock{mut};
  std::ostringstream os;

  os << name;
  if(running) {
    os << "(";
    os << std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now() - start_time
    ).count();
    os << "s)";
  }

  return os.str();
}
