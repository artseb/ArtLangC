#ifndef ART_STRING_H
#define ART_STRING_H

#include "value.h"
#include "state.h"

// ============================================================
// string — UTF-16 strings, interning, and String methods
//
// Files in this folder:
//   string_runtime.c   conversion, interning, concat, substring
//   string_methods.c   base methods + method registration entry
//   string_find.c      find, match, gmatch (pattern wrappers)
//   string_gsub.c      gsub (pattern wrapper)
//   string_feature.c   the Feature struct
//
// The pattern engine itself lives in core/syntax/pattern.c —
// it's generic infrastructure, not string-specific.
// ============================================================

typedef struct Feature Feature;
extern Feature string_feature;

// Register the pattern-based methods on the String class.
// Called from string_methods.c after the base methods are set.
void art_register_find_methods(ArtState *S, ObjClass *klass);
void art_register_gsub_methods(ArtState *S, ObjClass *klass);

#endif // ART_STRING_H