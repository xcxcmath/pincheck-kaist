INCLUDE = include
SRC = src
BUILD = build

CC = c++
CXXFLAGS = -std=c++17 -I$(INCLUDE) -O2 -W -Wall

NAME = pincheck
PROG = $(BUILD)/$(NAME)
MODULES = execution test_path test_runner test_result gdb_runner string_helper

define module_compile
$(BUILD)/$1.o : $(SRC)/$1.cpp $(INCLUDE)/$1.h $(INCLUDE)/common.h | $(BUILD)
	$(CC) $(CXXFLAGS) -c $$< -o $$@

endef

.PHONY: all run clean install #uninstall

all: $(PROG)

run: $(PROG)
	./$<

#install: $(PROG)
#	cp $(PROG) ../src/utils

#uninstall:
#	rm ../src/utils/$(NAME)

clean:
	rm -rf $(BUILD)

install: $(PROG)
	@PINTOSPATH=$$(dirname `which pintos`);\
	if [ -d "$$PINTOSPATH" ]; then cp $(PROG) $$PINTOSPATH ;\
	else echo "Cannot find pintos" ; exit 1; fi

$(PROG): $(PROG).o $(addprefix $(BUILD)/,$(addsuffix .o,$(MODULES)))
	$(CC) $(CXXFLAGS) -o $@ $^ -lstdc++fs -lpthread

$(PROG).o: $(SRC)/$(NAME).cpp $(INCLUDE)/common.h | $(BUILD)
	$(CC) $(CXXFLAGS) -c $< -o $@

$(foreach mod,$(MODULES),$(eval $(call module_compile,$(mod))))

$(BUILD):
	mkdir $(BUILD)