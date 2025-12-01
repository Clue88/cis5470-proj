MAKEFLAGS += --no-builtin-rules

PASS_ROOT ?= ../..
# Default to the plugin that the project builds: build/PointerAnalysis.so
PASS_SO   ?= $(PASS_ROOT)/build/PointerAnalysis.so

SUPPORT_DIR ?= ../testcasesupport

# Common flags for emitting LLVM IR
COMMON_FLAGS := -emit-llvm -S -fno-discard-value-names -Xclang -disable-O0-optnone \
                -I$(SUPPORT_DIR)

CFLAGS  := $(COMMON_FLAGS)
CXXFLAGS := $(COMMON_FLAGS) -std=c++11

# All C and C++ files in CWE476 subdir
SRC_C   := $(wildcard CWE476_*.c) $(wildcard */CWE476_*.c)
SRC_CPP := $(wildcard CWE476_*.cpp) $(wildcard */CWE476_*.cpp)

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
	@echo "== opt + PointerAnalysis on $< =="
	opt -load-pass-plugin=$(PASS_SO) -passes="PointerAnalysis" $< -disable-output \
	  > $@ 2>$(patsubst %.out,%.err,$@)
	@echo

clean:
	rm -f $(LL) $(OUT) $(ERR)
