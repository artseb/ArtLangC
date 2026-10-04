#ifndef ART_BUILTINS_H
#define ART_BUILTINS_H

// ============================================================
// builtins.h — compatibility shim
//
// The builtins/ folder was dissolved in the refactor. Every
// builtin now lives under art/features/, registered through
// Feature.register_builtins hooks.
//
// This header exists so that any code written against the old
// layout (tests, embedders) keeps compiling. It is a one-line
// re-export of the features header, which declares
// art_register_builtins.
// ============================================================

#include "features/features.h"

#endif // ART_BUILTINS_H
