#include "arg_parse.h"
#include "execution.h"

void parse_args(argparse::ArgumentParser &program, int argc, char *argv[]){
  std::ostringstream panic_msg;

  program.add_argument("-p", "--project")
         .help("Pintos project to run test; threads, userprog, vm, or filesys");
  program.add_argument("-j", "--jobs")
         .help("Maximum number of parallel test execution")
         .scan<'i', unsigned>()
         .default_value(HARDWARE_CONCURRENCY);
  program.add_argument("--verbose", "-V")
         .help("Verbose print")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-cb", "--clean-build")
         .help("Clean build directory before test")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-t", "--test", "--")
         .help("Test name; can be given multiple times")
         .default_value(Vector<String>{"*"})
         .append();
  program.add_argument("-sd", "--subdir")
         .help("Test subdir; can be given multiple times")
         .default_value(Vector<String>{"*"})
         .append();
  program.add_argument("-e", "--exclude", "--except")
         .help("Test name to be excluded; can be given multiple times")
         .default_value(Vector<String>{})
         .append();
  program.add_argument("-sde", "--subdir-exclude", "--subdir-except")
         .help("Test subdir to be excluded; can be given multiple times")
         .default_value(Vector<String>{})
         .append();
  program.add_argument("-S", "--sort")
         .help("Sort test cases first in decreasing order of TIMEOUT, which may help to check all faster")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-jr", "--just-run")
         .help("Run a case getting the output; only one at a time is required");
  program.add_argument("-gr", "--gdb-run")
         .help("Run a case getting the output with embedded GDB REPL; only one at a time is required");
  program.add_argument("-r", "--repeat")
         .help("# of repeating the whole checking")
         .scan<'i', unsigned>()
         .default_value(static_cast<unsigned>(1));
  program.add_argument("--gdb")
         .help("Run with --gdb option for pintos; used with --just-run")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("--with-timeout")
         .help("Run with TIMEOUT (-T option) for pintos; used with --just-run")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-fv")
         .help("Force to check new release is available")
         .default_value(false)
         .implicit_value(true);
  program.add_argument("-nv")
         .help("Not to check new release is available")
         .default_value(false)
         .implicit_value(true);
  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& e) {
    panic_msg << "Arguments parsing failed: " << e.what();
    panic(panic_msg);
  }
}