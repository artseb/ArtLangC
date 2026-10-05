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
    art/builtins/builtins.h \
    art/features/class/class_eval.c \
    art/features/class/class_ast_free.c \
    art/features/enum/enum_ast_free.c \
    art/features/interface/interface_ast_free.c \
    art/features/switch/switch_ast_free.c \
    art/features/const/const_eval.c \
    art/features/const/const_feature.c \
    art/features/const/const_parse.c \
    art/features/string/string_pattern.c

ifneq ($(wildcard $(STALE)),)
$(error Stale file(s) present: $(wildcard $(STALE)). These were moved, \
split, or merged. Run: rm $(wildcard $(STALE)))
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

.PHONY: all test clean

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

clean:
	rm -f $(CORE_OBJ) $(CORE_DEP)
	rm -f $(TEST_SRC:.c=.o) $(TEST_DEP) $(TEST_BIN)
	rm -f $(BIN)
	-rmdir bin 2>/dev/null || true

-include $(CORE_DEP)
-include $(TEST_DEP)
