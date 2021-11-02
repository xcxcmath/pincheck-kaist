#include <iostream>

#include "check_runner.h"
#include "test_runner.h"
#include "test_result.h"
#include "console_helper.h"
#include "termcolor/termcolor.hpp"

int check_run(const TestPath &paths, const Vector<TestCase> &target_tests, const Vector<TestCase> &persistence_tests,
  bool is_verbose, unsigned pool_size, unsigned repeats) {
  using namespace std::string_literals;

  const auto full_test_size = target_tests.size() + 2 * persistence_tests.size();

  const String omit_msg = " ... ";
  constexpr auto COL_JITTER = 3;

  unsigned epoch_passed = 0;

  for(unsigned epoch = 1; epoch <= repeats; ++epoch){
  Vector<TestResult> results, results_cache;
  Vector<std::unique_ptr<TestRunner>> pool(pool_size);

  size_t next = 0, next_pers = 0;

  if(repeats > 1) {
    std::cout << termcolor::bold << "\nEpoch " << epoch << " of " << repeats << termcolor::reset;
    std::cout << " (so far: " << termcolor::green << epoch_passed << " epochs passed, "
      << termcolor::red << (epoch - epoch_passed - 1) << " epochs failed" << termcolor::reset << ")" << std::endl;
  }
  
  // init pool print
  std::cout << std::endl;
  while(results.size() < full_test_size) {
    for(size_t i = 0; i < pool_size; ++i) {
      if(pool[i]) {
        if(pool[i]->is_finished()) {
          auto v = pool[i]->get_results();
          for(auto& u : v) {
            results_cache.emplace_back(std::move(u));
          }
          pool[i] = nullptr;
        }
      }
    }

    // check persistence test is executing
    bool can_execute_persistence = next_pers < persistence_tests.size();
    if(can_execute_persistence) {
      for(size_t i = 0; i < pool_size; ++i) {
        if(pool[i]) {
          if(pool[i]->get_test_case().persistence) {
            can_execute_persistence = false;
            break;
          }
        }
      }
    }

    for(size_t i = 0; i < pool_size; ++i) {
      if(pool[i]) continue;
      if(can_execute_persistence) {
        pool[i] = std::make_unique<TestRunner>(persistence_tests[next_pers]);
        pool[i]->register_test(paths);
        ++next_pers;
        can_execute_persistence = false;
      } else if(next < target_tests.size()){
        pool[i] = std::make_unique<TestRunner>(target_tests[next]);
        pool[i]->register_test(paths);
        ++next;
      }
    }

    if(!results_cache.empty()) {
      std::cout << "\033[2K\033[1G";
      for(const auto& r : results_cache) {
        r.print_row(true, is_verbose);
        std::cout << std::endl;
        results.emplace_back(r);
      }
      results_cache.clear();
    }

    const auto running_pools = std::count_if(pool.cbegin(), pool.cend(),
      [](const std::unique_ptr<TestRunner>& p){return p!=nullptr;});
    const String full_pool_msg = "Running"s + "(" + std::to_string(running_pools) + "/" + std::to_string(pool_size) + ") : ";
    std::cout << "\033[2K\033[1G";
    std::cout << termcolor::reset << termcolor::bold << termcolor::yellow
      << full_pool_msg;
    std::cout << termcolor::reset << termcolor::yellow;
    std::string pool_str;
    for(auto& p : pool) {
      if(!p) continue;
      if(p->get_test_case().persistence) {
        pool_str += "\033[35m"s + p->get_print() + "\033[33m ";
      } else {
        pool_str += p->get_print() + " ";
      }
      if (const size_t ws_col = get_winsize().ws_col;
        pool_str.size() + full_pool_msg.size() + COL_JITTER >= ws_col) {
        pool_str = pool_str.substr(0, ws_col - full_pool_msg.size() - COL_JITTER - omit_msg.size());
        pool_str += omit_msg;
        break;
      }
    }
    std::cout << pool_str << termcolor::reset << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  unsigned passed = std::count_if(results.begin(), results.end(),
    [](const TestResult& r){return r.passed;});
  unsigned failed = full_test_size - passed;

  std::cout << "\033[2K\033[1G";
  std::cout << "\nFinished total " << termcolor::bold << full_test_size << " tests." << termcolor::reset << std::endl;
  
  const bool all_passed = (passed == full_test_size);

  if (!all_passed) {
    std::cout << "\n" << termcolor::bright_red << "-- Failed tests --" << termcolor::reset << std::endl;
    for (const auto& tr : results) {
      if(!tr.passed) {
        tr.print_row(is_verbose, is_verbose);
        std::cout << std::endl;
      }
    }
    std::cout << std::endl;
  }
  std::cout << termcolor::green << "Pass: ";
  if(passed != 0) std::cout << termcolor::bold;
  std::cout << passed << '\t';

  std::cout << termcolor::reset << termcolor::red << "Fail: ";
  if(failed != 0) std::cout << termcolor::bold;
  std::cout << failed;
  std::cout << termcolor::reset << std::endl << std::endl;

  if (all_passed) {
    std::cout << termcolor::blue << termcolor::bold << "Correct!" << termcolor::reset << std::endl;
    epoch_passed++;
  }

  } // for-loop of epoch

  bool all_epoch_passed = epoch_passed == repeats;
  int return_value = all_epoch_passed ? 0 : 1;
  if (repeats > 1) {
    unsigned epoch_failed = repeats - epoch_passed;
    std::cout << std::endl;
    std::cout << termcolor::bold << "All " << repeats << " trials done." << std::endl;
    std::cout << termcolor::reset << termcolor::green << "All passing epochs: ";
    if(epoch_passed != 0) std::cout << termcolor::bold;
    std::cout << epoch_passed << std::endl;

    std::cout << termcolor::reset << termcolor::red << "Failed epochs: ";
    if(epoch_failed != 0) std::cout << termcolor::bold;
    std::cout << epoch_failed;
    std::cout << termcolor::reset << std::endl << std::endl;

    if (all_epoch_passed) {
      std::cout << termcolor::blue << termcolor::bold << "Succeeded for all trials!" << termcolor::reset << std::endl;
    }
  }

  return return_value;
}