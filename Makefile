# ============================================================
# ART — build file
#
# Wildcard source discovery: adding a new .c file under art/
# requires no edit here. Tests are discovered the same way.
#
# Portable across GCC and Clang on Linux, macOS, and MSYS2.
# The `-lm` flag is a no-op on macOS; it's needed on Linux
# where math lives in a separate library.
#
# Requirements: C11, GCC or Clang, libm (Linux only).
# ============================================================

CC      ?= gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O0 -MMD -MP \
           -iquoteart \
           -iquoteart/core/runtime \
           -iquoteart/core/syntax \
           -iquoteart/core/syntax/parser \
           -iquoteart/core/interp \
           -iquoteart/features
LDFLAGS := -lm

# --- Stale-file guard ----------------------------------------
#
# Files that were moved, merged, or renamed. If an old copy is
# still lying around (e.g. after unzipping a new tree over an
# old one) the wildcard source discovery below would pick it up
# and fail with duplicate symbols. Note art/features/features.h
# is also dangerous on Linux: -Iart/features would let it
# shadow glibc's <features.h>.

STALE := \
    art/core/runtime/obj_string.c \
    art/core/runtime/obj_table.c \
    art/core/runtime/obj_function.c \
    art/builtins/builtins.c \
    art/builtins/builtin_core.c \
    art/builtins/builtin_math.c \
    art/builtins/builtin_string.c \
    art/builtins/builtin_table.c \
    art/builtins/builtin_file.c \
    art/builtins/builtin_time.c \
    art/builtins/builtin_attempt.c \
    art/features/class/class_eval.c \
    art/features/class/class_ast_free.c \
    art/features/enum/enum_ast_free.c \
    art/features/interface/interface_ast_free.c \
    art/features/switch/switch_ast_free.c \
    art/features/const/const_eval.c \
    art/features/const/const_feature.c \
    art/features/const/const_parse.c \
    art/features/string/string_pattern.c \
    art/features/features.h \
    art/features/features.c \
    art/builtins/builtins.h \
    art/builtins.h \
    art/features/switch/switch_ast.c \
    art/features/switch/switch_parse.c \
    art/features/switch/switch_eval.c \
    art/features/switch/switch_feature.c \
    art/features/enum/enum_ast.c \
    art/features/enum/enum_parse.c \
    art/features/enum/enum_eval.c \
    art/features/enum/enum_runtime.c \
    art/features/enum/enum_gc.c \
    art/features/enum/enum_feature.c \
    art/features/interface/interface_ast.c \
    art/features/interface/interface_parse.c \
    art/features/interface/interface_eval.c \
    art/features/interface/interface_runtime.c \
    art/features/interface/interface_gc.c \
    art/features/interface/interface_feature.c \
    art/features/function/function_runtime.c \
    art/features/function/function_feature.c

# `make clean` is exempt: it deletes the stale files instead of
# refusing to run, so the fix is always one command.
ifneq ($(wildcard $(STALE)),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error Stale file(s) present: $(wildcard $(STALE)). These were moved, \
split, or merged. Run: make clean)
endif
endif

# --- Source discovery ----------------------------------------

CORE_SRC := \
    $(wildcard art/core/runtime/*.c) \
    $(wildcard art/core/syntax/*.c) \
    $(wildcard art/core/syntax/parser/*.c) \
    $(wildcard art/core/interp/*.c) \
    $(wildcard art/features/*.c) \
    $(wildcard art/features/*/*.c) \
    $(wildcard art/*.c)

CORE_OBJ := $(CORE_SRC:.c=.o)
CORE_DEP := $(CORE_OBJ:.o=.d)

TEST_SRC := $(wildcard tests/*.c)
TEST_DEP := $(TEST_SRC:.c=.d)
TEST_BIN := $(TEST_SRC:.c=)

# Tests must NOT link cli.c (it defines main()).
CORE_LIB_OBJ := $(filter-out art/cli.o,$(CORE_OBJ))

BIN := bin/art

# --- Targets --------------------------------------------------

.PHONY: all test test-asan clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(CORE_OBJ) | bin
	$(CC) $(CORE_OBJ) -o $@ $(LDFLAGS)

tests/%: tests/%.o $(CORE_LIB_OBJ)
	$(CC) $< $(CORE_LIB_OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bin:
	mkdir -p bin

test: all
	@set -e; \
	for t in $(TEST_BIN); do \
	    echo "=== $$t ==="; \
	    ./$$t; \
done
	@echo
	@echo "All test binaries passed."

# AddressSanitizer + UBSan run. Rebuilds everything with sanitizers,
# runs the suite, then cleans so normal builds aren't mixed with
# instrumented objects. Linux and macOS (MinGW has no ASan).
# Leak detection is off: the test programs leak small strings on
# purpose-less exit, which is noise, not interpreter bugs.
test-asan:
	$(MAKE) clean
	ASAN_OPTIONS=detect_leaks=0 $(MAKE) test \
	    CFLAGS="$(CFLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
	    LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"; \
	rc=$$?; $(MAKE) clean; exit $$rc

clean:
	rm -f $(wildcard $(STALE))
	rm -f $(CORE_OBJ) $(CORE_DEP)
	rm -f $(TEST_SRC:.c=.o) $(TEST_DEP) $(TEST_BIN)
	rm -f $(BIN)
	-rmdir bin 2>/dev/null || true

-include $(CORE_DEP)
-include $(TEST_DEP)
