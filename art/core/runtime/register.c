// ============================================================
// register.c — builtin-registration helpers
//
// Every C-implemented function or class method is bound through
// one of these four functions.
// ============================================================

#include <string.h>

#include "register.h"
#include "gc.h"

void art_define_native(ArtState *S, ObjTable *t, const char *name,
                       NativeFn fn, int arity)
{
    ObjString *n = obj_string_from_utf8(S, name, (int)strlen(name));
    GC_PUSH(S, OBJ_VAL(n));
    ObjNative *nat = obj_native_new(S, fn, n, arity);
    GC_POP(S, 1);

    GC_PUSH(S, OBJ_VAL(nat));
    table_set(S, t, n, OBJ_VAL(nat));
    GC_POP(S, 1);
}

void art_define_method(ArtState *S, ObjClass *klass, const char *name,
                       NativeFn fn, int arity)
{
    ObjString *n = obj_string_from_utf8(S, name, (int)strlen(name));
    GC_PUSH(S, OBJ_VAL(n));

    ObjNative *nat = obj_native_new(S, fn, n, arity);
    GC_PUSH(S, OBJ_VAL(nat));

    Value existing = table_get(klass->methods, n);
    ObjTable *overloads;

    if (IS_TABLE(existing))
    {
        overloads = AS_TABLE(existing);
    }
    else
    {
        overloads = obj_table_new(S);
        GC_PUSH(S, OBJ_VAL(overloads));
        table_set(S, klass->methods, n, OBJ_VAL(overloads));
        GC_POP(S, 1);
    }

    table_push(S, overloads, OBJ_VAL(nat));
    GC_POP(S, 2);
}

void art_define_value(ArtState *S, ObjTable *t, const char *name, Value v)
{
    ObjString *n = obj_string_from_utf8(S, name, (int)strlen(name));
    GC_PUSH(S, OBJ_VAL(n));
    table_set(S, t, n, v);
    GC_POP(S, 1);
}

void art_define_global(ArtState *S, const char *name, Value v)
{
    art_define_value(S, S->global_scope->vars, name, v);
}
