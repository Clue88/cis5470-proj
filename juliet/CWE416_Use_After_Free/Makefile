MAKEFLAGS += --no-builtin-rules

PASS_ROOT ?= ../..
PASS_SO   ?= $(PASS_ROOT)/build/UseAfterFreePass.so

SUPPORT_DIR ?= ../testcasesupport

# Common flags for emitting LLVM IR
COMMON_FLAGS := -emit-llvm -S -fno-discard-value-names -Xclang -disable-O0-optnone \
                -I$(SUPPORT_DIR)

CFLAGS  := $(COMMON_FLAGS)
CXXFLAGS := $(COMMON_FLAGS) -std=c++11

# All CWE416 C and C++ files in sXX subdirs
SRC_C   := $(wildcard CWE416_Use_After_Free__*.c)
SRC_CPP := $(wildcard CWE416_Use_After_Free__*.cpp)

SRC := $(SRC_C) $(SRC_CPP)

# Derive .ll/.out/.err names alongside the sources
LL  := $(SRC:.c=.ll)
LL  := $(LL:.cpp=.ll)

OUT := $(SRC:.c=.out)
OUT := $(OUT:.cpp=.out)

ERR := $(SRC:.c=.err)
ERR := $(ERR:.cpp=.err)

all: $(OUT)

%.ll: %.c
	@echo "== clang (C) $< =="
	clang $(CFLAGS) -c -o $@ $<

%.ll: %.cpp
	@echo "== clang++ (C++) $< =="
	clang++ $(CXXFLAGS) -c -o $@ $<

%.out: %.ll
	@echo "== opt + UseAfterFree on $< =="
	opt -load-pass-plugin=$(PASS_SO) -passes="UseAfterFree" $< -disable-output \
	  > $@ 2>$(patsubst %.out,%.err,$@)
	@echo

clean:
	rm -f $(LL) $(OUT) $(ERR)
