# Pincheck KAIST

Parallel testing for [casys-kaist/pintos-kaist](https://github.com/casys-kaist/pintos-kaist) projects
... with several features.

**Testing with `persistence` of project 4 is currently highly experimental. Please let me know any issue about it.**

![thumb_threads](./images/thumb_threads.gif)

## Index

- [README](#readme)
- [Acknowledgement](#acknowledgement)
- [Features](#features)
  - [Testing](#testing)
  - [Running](#running)
  - [Debugging](#debugging)
- [Usage](#usage)
  - [Testing](#for-testing)
  - [Running](#for-running)
  - [Debugging](#for-debugging)
  - [Misc.](#misc)
- [Benchmarks](#benchmark-with-all-passing-implementation)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Update](#update)

## README

- As this utility is still in progress to complete, there would be many errors, bugs, or
  glitches. Share your bad experiences with `pincheck` if any, through KLMS or Github issues tab.
- The original `make check` command is always more accurate.
- If you give `-j` option to be much higher than the number of machine's cores,
some test cases might be failed even if it is correct.
- This tool doesn't work with original Pintos.

## Acknowledgement

- [ikalnytskyi/termcolor](https://github.com/ikalnytskyi/termcolor)
- [p-ranav/argparse](https://github.com/p-ranav/argparse)

## Features

### Testing

![thumb_userprog](./images/thumb_userprog.gif)

- Test using **multi-threads**, with more **visual cues**
- **Test target filtering** with wildcards (`*`, `?`)
- Running test of **any project** in **any path**

![thumb_repeat](./images/thumb_repeat.gif)

- **Repeating** the whole checking process multiple times

### Running

![thumb_just_run](./images/thumb_just_run.gif)

- **Running one case** to see only the output, but with additional **`backtrace` detection**

### Debugging

![thumb_gdb_fun](./images/thumb_gdb_run.gif)

- **Embedded GDB REPL;** you don't need to make 2 terminal windows nor type `target remote localhost:1234`

## Usage

### For testing

```sh
# Run tests of threads project
pintos-kaist/src/threads$ pincheck

# Run tests of userprog project (can be executed in other directories)
$ pincheck -p userprog

# Run tests with 3 parallel test pool
# The default value is the number of hardware cores(threads)
pintos-kaist/src/threads$ pincheck -j 3

# Run "alarm-single" test only
pintos-kaist/src/threads$ pincheck -- alarm-single

# Run tests whose names have "alarm-" or "priority-" prefix
pintos-kaist/src/threads$ pincheck -- alarm-* -- priority-*

# Run tests except the tests with "mlfqs" prefix
pintos-kaist/src/threads$ pincheck --exclude mlfqs*

# Run tests which belong to "...../mlfqs" test subdirectory
pintos-kaist/src/threads$ pincheck --subdir */mlfqs

# Run tests which don't belong to "...../mlfqs" test subdirectory
pintos-kaist/src/threads$ pincheck --subdir-exclude */mlfqs

# Run tests in decreasing order of TIMEOUT, which may make the whole process faster
pintos-kaist/src/userprog$ pincheck --sort

# Run tests after cleaning build directory
pintos-kaist/src/vm$ pincheck --clean-build
pintos-kaist/src/vm$ pincheck -cb

# Repeat the whole tests 5 times
pintos-kaist/src/filesys$ pincheck --repeat 5
```

### For running

```sh
# Just run to see the outputs and possible backtraces
$ pincheck --just-run alarm-single
$ pincheck -jr alarm-single

# You can pass as the full path, too.
$ pincheck --just-run tests/userprog/dup2/dup2-complex

# Just run, but with its timeout option
$ pincheck --just-run multi-oom --with-timeout

# Just run, but with --gdb option
$ pincheck --just-run syn-read --gdb
```

This is the example running for `--gdb` option:

![thumb_gdb](./images/thumb_gdb.gif)

### For debugging

```sh
# GDB REPL
$ pincheck --gdb-run write-normal
$ pincheck -gr mmap-bad-off
```

### Misc.

```sh
# Run with verbose
$ pincheck --verbose
$ pincheck -V

# Force checking new release when start
$ pincheck -fv

# No checking new release
$ pincheck -nv

# Note : By default, pincheck SOMETIMES (25%) checks its new release.

# version & help
$ pincheck --version
$ pincheck -v

$ pincheck --help
$ pincheck -h
```

## Benchmark with all passing implementation

For `pincheck`, the `-nv` option is omitted here.

### `threads`

On KCloud VM (2 cores, 4GB RAM, 100GB Storage, Ubuntu 18.04)

- `make check` : 842 seconds
- `make check -j 2` : 423 seconds
- `pincheck (-j 2)` : 426 seconds
- 1 MLFQS test failed for both `make` and `pincheck` with `-j 3` or more.

On my VirtualBox (VM with 4 cores, 8GB RAM, 20GB Storage, Ubuntu 18.04)

- `make check` : 901 seconds
- `make check -j 4` : 227 seconds
- `pincheck (-j 4)` : 227 seconds
- `make check -j 5` : 197 seconds
- `pincheck -j 5` : 197 seconds

Note that the longest test to finish (`mlfqs-recent-1`) takes about 190 seconds.

### `userprog` (including `dup2`)

On KCloud VM (2 cores, 4GB RAM, 100GB Storage, Ubuntu 18.04)

- `make check` : 192 seconds
- `make check -j 2` : 99 seconds
- `pincheck (-j 2)` : 108 seconds
- `make check -j 3` : 90 seconds
- `pincheck -j 3` : 99 seconds

On my VirtualBox (VM with 4 cores, 8GB RAM, 20GB Storage, Ubuntu 18.04)

- `make check` : 181 seconds
- `make check -j 4` : 64 seconds
- `pincheck (-j 4)` : 65 seconds
- `pincheck (-j 4) --sort` : 49 seconds
- `make check -j 5` : 58 seconds
- `pincheck -j 5` : 64 seconds
- `make check -j 8` : 56 seconds
- `pincheck -j 8` : 57 seconds
- `pincheck -j 8 --sort` : 52 seconds

### `vm` (including `cow-simple`)

On KCloud VM (2 cores, 4GB RAM, 100GB Storage, Ubuntu 18.04)

- `make check` : 323 seconds
- `make check -j 2` : 141 seconds
- `pincheck (-j 2)` : 175 seconds
- `make check -j 3` : 122 seconds
- `pincheck -j 3` : 145 seconds

On my VirtualBox (VM with 4 cores, 8GB RAM, 20GB Storage, Ubuntu 18.04)

- `make check` : 297 seconds
- `make check -j 4` : 81 seconds
- `pincheck (-j 4)` : 93 seconds
- `pincheck (-j 4) --sort` : 73 seconds
- `make check -j 5` : 76 seconds
- `pincheck -j 5` : 78 seconds
- `make check -j 8` : 65 seconds
- `pincheck -j 8` : 72 seconds
- `pincheck -j 8 --sort` : 58 seconds

### `filesys`

To be tested soon.

## Prerequisites

**Please read carefully.**

### GCC 8.3.0 or later

Any C++ compiler that supports C++17 well with file system library would be enough.
Here shows how to configure `g++-8`, for example, on an Ubuntu 18.04 instance of KCloud VM.

```sh
# on Ubuntu 18.04 or later
$ sudo apt update
$ sudo apt install g++-8 # or later
$ sudo update-alternatives --display g++
# if no alternatives for g++ found, run this
$ sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 40

$ sudo update-alternatives --display c++
# if no alternatives for c++ found, run this
$ sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 40

# version check
$ c++ --version
```

If you don't want to use `update-alternatives`, please change the value of `CC` in `Makefile` from `c++` to `g++`, `g++-8`, or etc.

### `pintos` in `PATH` variable

The pintos utils directory MUST BE registered in `PATH` environment variable in advance, as `pincheck` detects your pintos project directory by executing `which pintos` command.

```sh
# write following in .bashrc
# or execute manually in advance
source (pintos directory)/activate
```

If you use KCloud VM, it is likely to be something like `/root/pintos-kaist` or `/home/<your id>/pintos-kaist`.

```sh
source /root/pintos-kaist/activate
# or
source /home/<your id>/pintos-kaist/activate
```

Afterwards, you should get output by running `which pintos` on the shell. If nothing printed, please read above carefully.

### `curl` or `wget` (recommended)

(From version v21.10.05) `pincheck` can search its new releases through Github API, using `curl` or `wget` as at least one of them is available. If none of them is installed, please run on Ubuntu:

```sh
$ sudo apt update

$ sudo apt install curl
# or
$ sudo apt install wget
```

### CPU with 2+ cores (recommended)

`pincheck` basically utilize multi-thread programming, but thanks to the standard library, it also tries to be moderate; it may defer generating new threads if the resource is running out.

Please make your host machine be powerful enough to deal with mutiple simultaneous tests, otherwise some of tests might fail even if your implementation is correct.

## Installation

> We recommend `git clone` and `make install`, as it can do many things for you.

**Please read carefully.**

First, clone this repo somewhere.

```sh
$ git clone https://github.com/xcxcmath/pincheck-kaist
```

Second, build the executable and copy to your pintos' `utils` directory.

```sh
$ cd pincheck-kaist
$ make install
```

**We highly recommend to use `make install`, because**

- `make install` detects your pintos project directory;
- `make install` uses multi-core compiling with machine's hardware cores to make building faster; and
- `make install` makes sure that all object files can be (re)built and linked correctly.

After `make install`, you should be able to see a green message: `pincheck installed at <installed path>`.

If red `Cannot find pintos ...` message appears when running `make install`, please check whether you can run command `pintos`.

As you are using git for your project, you may add `pincheck` to `.gitignore`.

If you want to copy pincheck in other directory,

```sh
$ cp build/pincheck /home/<your name>/pintos-kaist/utils
```

Actually, you can use `pincheck` executable anyway without copying, but you may want to run it shortly.

```sh
<dir of pincheck-kaist>/build/pincheck -p filesys
```

Another option is editing `PATH` in `.bashrc` adding the build directory.

```sh
# in ~/.bashrc
export PATH="$PATH:(dir of pincheck-kaist)/build"
```

## Update

You may notice, sometimes, a message that notify new release of `pincheck` when you run `pincheck`.
Then, please run in `pincheck-kaist` directory:

```sh
pincheck-kaist$ git pull
pincheck-kaist$ make install
```
