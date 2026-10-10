#ifndef ART_H
#define ART_H

#include <stdbool.h>
#include "value.h"

// ============================================================
// ART embed API
//
// Every entry point takes an opaque ArtState. One instance per
// host (game engine, script runner, test harness). No global
// mutable state lives outside ArtState, so multiple instances
// coexist in one process.
//
// Errors are reported to stderr immediately in the format
//   file:line:col: error: message
// and the run* functions return false. The state is reset for
// the next call, so an embed can keep going after a bad script.
// ============================================================

typedef struct ArtState ArtState;

// Create a fresh interpreter with the standard builtins loaded.
ArtState *art_open(void);

// Free everything. S is invalid afterward.
void art_close(ArtState *S);

// Run a source string. `name` appears in error messages; use
// "<string>" or the actual filename. Returns true on success.
bool art_run_string(ArtState *S, const char *source, const char *name);

// Read a file and run it. Returns false on I/O or runtime error.
bool art_run_file(ArtState *S, const char *path);

// ============================================================
// Native function registration
//
// A native function is a C callback that ART scripts call like
// any other function. It receives the argument count, an array
// of Values, and returns a Value.
//
//   static Value my_log(ArtState *S, int argc, Value *argv)
//   {
//       for (int i = 0; i < argc; i++) {
//           ObjString *s = value_to_string(S, argv[i]);
//           char *u = obj_string_to_utf8(s);
//           fputs(u, stdout);
//           free(u);
//       }
//       fputc('\n', stdout);
//       return NIL_VAL;
//   }
//
//   art_register_native(S, "Log", my_log, -1);
//
// Arity: a non-negative integer requires exactly that many
// arguments; -1 means variadic. Registration is global-scope
// only. To install methods on a class, use the internal
// `art_define_method` from `core/runtime/register.h`.
//
// The NativeFn type is defined in value.h as:
//   typedef Value (*NativeFn)(ArtState *S, int argc, Value *argv);
// ============================================================

void art_register_native(ArtState *S, const char *name,
                         NativeFn fn, int arity);

#endif // ART_H
