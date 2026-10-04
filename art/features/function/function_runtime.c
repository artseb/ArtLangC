// ============================================================
// function_runtime.c — constructors + shared arity helpers
// ============================================================

#include <stdlib.h>
#include "function.h"
#include "gc.h"

ObjFunction *obj_function_new(ArtState *S, ObjString *name)
{
    ObjFunction *fn = ALLOCATE_OBJ(S, ObjFunction, OBJ_FUNCTION);
    fn->name = name;
    fn->arity = 0;
    fn->params = NULL;
    fn->body = NULL;
    fn->owner_class = NULL;
    fn->is_static = false;
    fn->is_constructor = false;
    fn->is_getter = false;
    fn->is_setter = false;
    fn->is_operator = false;
    fn->operator_op = -1;
    fn->is_private = false;
    fn->is_variadic = false;
    return fn;
}

ObjNative *obj_native_new(ArtState *S, NativeFn fn, ObjString *name, int arity)
{
    ObjNative *nat = ALLOCATE_OBJ(S, ObjNative, OBJ_NATIVE);
    nat->fn = fn;
    nat->name = name;
    nat->arity = arity;
    return nat;
}

ObjClosure *obj_closure_new(ArtState *S, ObjFunction *fn)
{
    ObjClosure *cl = ALLOCATE_OBJ(S, ObjClosure, OBJ_CLOSURE);
    cl->function = fn;
    cl->captured_scope = NULL;
    cl->owner_class = NULL;
    return cl;
}

int function_min_arity(ObjFunction *fn)
{
    int n = fn->arity;
    while (n > 0 && fn->params[n - 1].default_value != NULL)
        n--;
    return n;
}

bool function_accepts_arity(ObjFunction *fn, int argc)
{
    int min = function_min_arity(fn);
    if (fn->is_variadic)
        return argc >= min;
    return argc >= min && argc <= fn->arity;
}
