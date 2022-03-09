##
## Makefile for Miosix embedded OS
##
MAKEFILE_VERSION := 1.09
GCCMAJOR := $(shell arm-miosix-eabi-gcc --version | \
                    perl -e '$$_=<>;/\(GCC\) (\d+)/;print "$$1"')
## Path to kernel directory (edited by init_project_out_of_git_repo.pl)
KPATH := miosix-kernel-unofficial/miosix/
## Path to config directory (edited by init_project_out_of_git_repo.pl)
CONFPATH := ./
include $(CONFPATH)/config/Makefile.inc

##
## List here subdirectories which contains makefiles
##
SUBDIRS := $(KPATH)

##
## List here your source files (both .s, .c and .cpp)
##
SRC :=                                  \
main.cpp

NET_SRC :=\
$(wildcard network_module/*.cpp) $(wildcard network_module/*/*.cpp) $(wildcard network_module/*/*/*.cpp) $(wildcard network_module/*/*/*/*.cpp)

##
## List here additional static libraries with relative path
##
LIBS :=

##
## List here additional include directories (in the form -Iinclude_dir)
##
INCLUDE_DIRS :=

##
## Command line to flash WandStem nodes
##
PROGRAM_SCRIPT_PATH   := scripts/wandstem-flash-utility/build
PROGRAM_EXEC_FILE     := main.bin
PROGRAM_SCRIPT_PARAMS := -m u -f $(PROGRAM_EXEC_FILE)
PROGRAM_CMDLINE       := $(PROGRAM_SCRIPT_PATH)/wandstem-flash $(PROGRAM_SCRIPT_PARAMS)

##############################################################################
## You should not need to modify anything below                             ##
##############################################################################

ifeq ("$(VERBOSE)","1")
Q := 
ECHO := @true
else
Q := @
ECHO := @echo
endif

## Test files

TEST_LPATH := tests/local
TEST_BPATH := tests/board
TEST_LFILES := $(addsuffix .local, $(wildcard $(TEST_LPATH)/*.cpp) $(wildcard $(TEST_LPATH)/**/*.cpp))
TEST_BFILES := $(wildcard $(TEST_BPATH)/*.cpp) $(wildcard $(TEST_BPATH)/**/*.cpp)

## Replaces both "foo.cpp"-->"foo.o" and "foo.c"-->"foo.o"
OBJ := $(addsuffix .o, $(basename $(SRC)))
NET_OBJ := $(addsuffix .o, $(basename $(NET_SRC)))
TEST_LOBJ := $(addsuffix .o.local, $(basename $(basename $(TEST_LFILES))))
TEST_BBINS := $(addsuffix .bin, $(basename $(TEST_BFILES)))

## From compiler prefix form the name of the compiler and other tools
LCC  := gcc
LCXX := g++
LLD  := ld
LAR  := ar
LAS  := as
LCP  := objcopy
LOD  := objdump
LSZ  := size

## Includes the miosix base directory for C/C++
## Always include CONFPATH first, as it overrides the config file location
CXXFLAGS := $(CXXFLAGS_BASE) -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC)  \
            -I. -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)      \
            -I$(KPATH)/$(BOARD_INC) $(INCLUDE_DIRS)
CFLAGS   := $(CFLAGS_BASE)   -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC)  \
            -I. -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)      \
            -I$(KPATH)/$(BOARD_INC) $(INCLUDE_DIRS)
AFLAGS   := $(AFLAGS_BASE)
LFLAGS   := $(LFLAGS_BASE)
DFLAGS   := -MMD -MP

LDFLAGS   := -MMD -MP
LCXXFLAGS := -O0 -Wall -g

## libmiosix.a is among stdlibs because needs to be within start/end group
STDLIBS  := -lmiosix -lstdc++ -lc -lm -lgcc

ifneq ($(GCCMAJOR),4)
	STDLIBS += -latomic
endif

LINK_LIBS := $(LIBS) -L$(KPATH) -Wl,--start-group $(STDLIBS) -Wl,--end-group

all: all-recursive main

clean: clean-recursive clean-topdir

program:
	$(PROGRAM_CMDLINE)

all-recursive:
	$(foreach i,$(SUBDIRS),$(MAKE) -C $(i)                               \
	  KPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(KPATH))       \
	  CONFPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(CONFPATH)) \
	  || exit 1;)

clean-recursive:
	$(foreach i,$(SUBDIRS),$(MAKE) -C $(i)                               \
	  KPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(KPATH))       \
	  CONFPATH=$(shell perl $(KPATH)/_tools/relpath.pl $(i) $(CONFPATH)) \
	  clean || exit 1;)

clean-topdir:
	-rm -f $(OBJ) $(NET_OBJ) main.elf main.hex main.bin main.map $(OBJ:.o=.d) $(NET_OBJ:.o=.d)

main: main.elf
	$(ECHO) "[CP  ] main.hex"
	$(Q)$(CP) -O ihex   main.elf main.hex
	$(ECHO) "[CP  ] main.bin"
	$(Q)$(CP) -O binary main.elf main.bin
	$(Q)$(SZ) main.elf

main.elf: $(OBJ) $(NET_OBJ) all-recursive
	$(ECHO) "[LD  ] main.elf"
	$(Q)$(CXX) $(LFLAGS) -o main.elf $(OBJ) $(NET_OBJ) $(KPATH)/$(BOOT_FILE) $(LINK_LIBS)

%.o: %.s
	$(ECHO) "[AS  ] $<"
	$(Q)$(AS)  $(AFLAGS) $< -o $@

%.o : %.c
	$(ECHO) "[CC  ] $<"
	$(Q)$(CC)  $(DFLAGS) $(CFLAGS) $< -o $@

%.o : %.cpp
	$(ECHO) "[CXX ] $<"
	$(Q)$(CXX) $(DFLAGS) $(CXXFLAGS) $< -o $@

test: testlocal testboard

testlocal: $(TEST_LOBJ)

%.o.local:
	$(ECHO) "[CXX ] $(addsuffix .cpp, $(basename $(basename $@)))"
	$(Q)$(LCXX) $(LDFLAGS) $(LCXXFLAGS) $(addsuffix .cpp, $(basename $(basename $@))) -o $(basename $@)

testboard: $(TEST_BBINS)

%.bin: %.elf
	$(ECHO) "[CP  ] $(addsuffix .hex, $(basename $@))"
	$(Q)$(CP) -O ihex $< $(addsuffix .hex, $(basename $@))
	$(ECHO) "[CP  ] $@"
	$(Q)$(CP) -O binary $< $@
	$(Q)$(SZ) $<

%.elf: %.o $(NET_OBJ) all-recursive
	$(ECHO) "[LD  ] $@"
	$(Q)$(CXX) $(LFLAGS) -o $@ $< $(NET_OBJ) $(KPATH)/$(BOOT_FILE) $(LINK_LIBS)

#pull in dependecy info for existing .o files
-include $(OBJ:.o=.d) $(NET_OBJ:.o=.d)
