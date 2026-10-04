#ifndef ART_H
#define ART_H

#include <stdbool.h>

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

#endif // ART_H
