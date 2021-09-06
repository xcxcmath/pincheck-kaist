#ifndef PINCHECK_EXECUTION_H
#define PINCHECK_EXECUTION_H

#include <sstream>
#include "common.h"

Pair<int, Optional<String>> exec_str(const char *cmd) noexcept;
int exec_ret(const char *cmd) noexcept;

extern const unsigned HARDWARE_CONCURRENCY;

[[noreturn]] void panic(const String& msg, int exit_code=1);
[[noreturn]] void panic(const std::ostringstream& os, int exit_code=1);


#endif
