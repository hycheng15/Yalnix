#
#	Sample Makefile for COMP 421 Yalnix kernel and user programs.
#
#	The Yalnix kernel built will be named "yalnix".  *ALL* kernel
#	Makefiles for this lab must have a "yalnix" rule in them, and
#	must produce a kernel executable named "yalnix" -- we will run
#	your Makefile and will grade the resulting executable
#	named "yalnix".
#
#	Your project must be implemented using the C programming
#	language (e.g., not in C++ or other languages).
#

#
#	Define the list of everything to be made by this Makefile.
#	The list should include "yalnix" (the name of your kernel),
#	plus the list of user test programs you also want to be mae
#	by this Makefile.  For example, the definition below
#	specifies to make Yalnix test user programs test1, test2,
#	and test3.  You should modify this list to the list of your
#	own test programs.
#
#	For each user test program, the Makefile will make the
#	program out of a single correspondingly named sourc file.
#	For example, the Makefile will make test1 out of test1.c,
#	if you have a file named test1.c in this directory.
#

TEST_DIR = tests
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(basename $(TEST_SRCS))

# ALL = yalnix idle init second
ALL = yalnix idle init second $(TEST_BINS)

#
#	You must modify the KERNEL_OBJS and KERNEL_SRCS definitions
#	below.  KERNEL_OBJS should be a list of the .o files that
#	make up your kernel, and KERNEL_SRCS should  be a list of
#	the corresponding source files that make up your kernel.
#
KERNEL_OBJS = load.o memory.o pcb.o syscalls.o trap_handler.o yalnix.o globals.o 
KERNEL_SRCS = load.c memory.c pcb.c syscalls.c trap_handler.c yalnix.c globals.c 

#
#	You should not have to modify anything else in this Makefile
#	below here.  If you want to, however, you may modify things
#	such as the definition of CFLAGS, for example.
#

PUBLIC_DIR = /clear/courses/comp421/pub

CPPFLAGS = -I$(PUBLIC_DIR)/include
CFLAGS = -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable

LANG = gcc

%: %.o
	$(LINK.o) -o $@ $^ $(LOADLIBES) $(LDLIBS)

LINK.o = $(PUBLIC_DIR)/bin/link-user-$(LANG) $(LDFLAGS) $(TARGET_ARCH)

%: %.c
%: %.cc
%: %.cpp

all: $(ALL)

yalnix: $(KERNEL_OBJS)
	$(PUBLIC_DIR)/bin/link-kernel-$(LANG) -o yalnix $(KERNEL_OBJS)

clean:
	rm -f $(KERNEL_OBJS) $(ALL)

depend:
	$(CC) $(CPPFLAGS) -M $(KERNEL_SRCS) > .depend

#include .depend
