#include "console_helper.h"

struct winsize get_winsize() {
  struct winsize winsize;
  ioctl(0, TIOCGWINSZ, &winsize);
  return winsize;
}