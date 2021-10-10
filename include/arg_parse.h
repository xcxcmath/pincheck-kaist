#ifndef PINCHECK_ARG_PARSE_H
#define PINCHECK_ARG_PARSE_H

#include "common.h"
#include "argparse/argparse.hpp"

void parse_args(argparse::ArgumentParser &program, int argc, char *argv[]);

#endif