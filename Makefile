NPROCS:=1
OS:=$(shell uname -s)

ifeq ($(OS),Linux)
	NPROCS:=$(shell grep -c ^processor /proc/cpuinfo)
endif
ifeq ($(OS),Darwin)
	NPROCS:=$(shell system_profiler | awk '/Number Of CPUs/{print $4}{next;}')
endif

INCLUDE = include
SRC = src
BUILD = build

CC = c++
CXXFLAGS = -std=c++17 -I$(INCLUDE) -O2 -W -Wall

NAME = pincheck
PROG = $(BUILD)/$(NAME)
MODULES = execution arg_parse version_check \
test_case test_path rubric_parse test_runner test_result \
check_runner just_runner gdb_runner \
string_helper console_helper

define module_compile
$(BUILD)/$1.o : $(SRC)/$1.cpp $(INCLUDE)/$1.h $(INCLUDE)/common.h | $(BUILD)
	$(CC) $(CXXFLAGS) -c $$< -o $$@

endef

.PHONY: all run clean install

all: $(PROG)

run: $(PROG)
	./$<

clean:
	rm -rf $(BUILD)

install:
	@$(MAKE) clean
	@$(MAKE) all -j $(NPROCS)
	@if [ -f "$(PROG)" ]; then echo "\nBuild completed" ; \
	else echo "\nCompile failed.." ; exit 1; fi
	@PINTOSPATH=$$(dirname `which pintos`);\
	if [ -d "$$PINTOSPATH" ]; then cp $(PROG) $$PINTOSPATH ; \
	echo "\033[32mpincheck installed at" $$PINTOSPATH ; \
	else \
	echo "\033[31mCannot find pintos. Did you set PATH variable to include pintos?" ; exit 1; fi ; \
	echo "\033[0m"

$(PROG): $(PROG).o $(addprefix $(BUILD)/,$(addsuffix .o,$(MODULES)))
	$(CC) $(CXXFLAGS) -o $@ $^ -lstdc++fs -lpthread

$(PROG).o: $(SRC)/$(NAME).cpp $(INCLUDE)/common.h | $(BUILD)
	$(CC) $(CXXFLAGS) -c $< -o $@

$(foreach mod,$(MODULES),$(eval $(call module_compile,$(mod))))

$(BUILD):
	mkdir $(BUILD)