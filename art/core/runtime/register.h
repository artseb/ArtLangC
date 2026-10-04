#ifndef ART_REGISTER_H
#define ART_REGISTER_H

#include "value.h"
#include "state.h"

// ============================================================
// register — the four builtin-registration helpers
//
// These used to live in builtins.c as shared statics. Now that
// builtins/ is gone and every builtin lives under features/,
// the helpers are part of the runtime API.
//
// Conventions:
//   - art_define_native and art_define_value replace any
//     existing binding for that name.
//   - art_define_method accumulates into an overload list,
//     matching how the class feature registers user-defined
//     overloads.
//   - art_define_global is art_define_value into the global
//     scope.
// ============================================================

void art_define_native(ArtState *S, ObjTable *t, const char *name,
                       NativeFn fn, int arity);

void art_define_method(ArtState *S, ObjClass *klass, const char *name,
                       NativeFn fn, int arity);

void art_define_value(ArtState *S, ObjTable *t, const char *name, Value v);

void art_define_global(ArtState *S, const char *name, Value v);

#endif // ART_REGISTER_H
