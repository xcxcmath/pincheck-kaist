#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include "termcolor/termcolor.hpp"
#include "execution.h"
#include "gdb_runner.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

constexpr char GDB_MACROS[] =
  "define debugpintos\n"
  "\ttarget remote localhost:1234\n"
  "\techo Press 'c' and Enter to start Pintos!\n"
  "\techo \\n\n"
  "end\n"
  "document debugpintos\n"
  "\tAttach debugger to pintos process\n"
  "end\n";

constexpr char CLIENT_COMMAND[] = "gdb -x gdb-macros -ex debugpintos kernel.o";

// https://stackoverflow.com/questions/548063/kill-a-process-started-with-popen
// https://stackoverflow.com/questions/36673972/non-blocking-read-on-pipe/36674490
pid_t popen2(const String &server_command, int &outfp, bool new_group) {
  int p_stdout[2];

  const int pipe_out_ret = pipe(p_stdout);
  if(pipe_out_ret != 0) {
    return -1;
  }

  pid_t pid = fork();

  if(pid < 0)
    return pid;
  else if (pid == 0) {
    if(new_group){
      setpgid(0, 0);
    }
    close(p_stdout[0]);
    dup2(p_stdout[1], STDOUT_FILENO);
    execl("/bin/sh", "sh", "-c", server_command.c_str(), NULL);
    exit(1);
  }

  outfp = p_stdout[0];

  fcntl(outfp, F_SETFL, fcntl(outfp, F_GETFL) | O_NONBLOCK);

  return pid;
}

int gdb_run(const String &server_command) {
  int status;
  std::ofstream gdb_macros{"gdb-macros"};
  if(!gdb_macros.is_open()){
    panic("Cannot make gdb-macros file.");
  }
  gdb_macros << GDB_MACROS;
  gdb_macros.close();

  int server_outfp;
  auto server_pid = popen2(server_command, server_outfp, true);
  if(server_pid < 0) {
    panic("Cannot create server process");
  }

  int client_outfp;
  auto client_pid = popen2(CLIENT_COMMAND, client_outfp, false);
  if(client_pid < 0) {
    kill(server_pid, SIGTERM);
    waitpid(server_pid, &status, 0);
    panic("Cannot create client process");
  }

  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  std::mutex cout_mut;
  std::atomic<bool> client_finished(false);
  std::thread print_thread([&cout_mut, &client_finished, &server_outfp, &client_outfp]{
    Buffer server_buf;
    Buffer client_buf;
    while(!client_finished.load()) {
      ssize_t server_r = read(server_outfp, server_buf.data(), server_buf.size()-1);
      ssize_t client_r = read(client_outfp, client_buf.data(), client_buf.size()-1);
      if(server_r > 0) {
        server_buf[server_r] = '\0';
        std::cout << server_buf.data() << std::flush;
      }

      if(client_r > 0) {
        client_buf[client_r] = '\0';
        if(server_r > 0) {
          std::cout << termcolor::on_yellow << "%\n";
        }
        std::cout << termcolor::yellow << client_buf.data() << termcolor::reset << std::flush;
      }
    }
  });

  waitpid(client_pid, &status, 0);
  int client_status = status;
  waitpid(server_pid, &status, 0);

  client_finished.store(true);
  print_thread.join();

  close(client_outfp);
  close(server_outfp);

  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);

  return (WIFEXITED(client_status) ? WEXITSTATUS(client_status) : -1);
}
