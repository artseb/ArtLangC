// ============================================================
// class_dispatch.c — method lookup and instantiation
// ============================================================

#include "class.h"
#include "interp.h"
#include "gc.h"
#include <stdlib.h>
#include <string.h>

static ObjClosure *find_overload(ObjTable *methods, ObjString *name,
                                 int argc, Value *args)
{
    Value v = table_get(methods, name);
    if (!IS_TABLE(v)) return NULL;
    ObjTable *overloads = AS_TABLE(v);

    for (int i = 0; i < overloads->array_count; i++)
    {
        ObjClosure *cl = AS_CLOSURE(overloads->array[i]);
        ObjFunction *fn = cl->function;

        if (!function_accepts_arity(fn, argc)) continue;

        int check_upto = argc < fn->arity ? argc : fn->arity;
        bool ok = true;
        for (int j = 0; j < check_upto; j++)
        {
            if (!value_matches_type(args[j],
                                    fn->params[j].type_name,
                                    fn->params[j].type_class,
                                    fn->params[j].type_interface))
            {
                ok = false;
                break;
            }
        }
        if (ok) return cl;
    }
    return NULL;
}

ObjClosure *class_find_method(ArtState *S, ObjClass *klass, ObjString *name,
                              int argc, Value *args, Node *at)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        ObjClosure *cl = find_overload(c->methods, name, argc, args);
        if (cl) return cl;
    }
    (void)S; (void)name; (void)at;
    return NULL;
}

ObjClosure *class_find_operator(ArtState *S, ObjClass *klass, TokenType op,
                                int argc, Value *args)
{
    (void)S;
    const char *opname = token_type_name(op);
    if (opname == NULL) return NULL;

    ObjString *key = NULL;

    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        if (key == NULL)
            key = obj_string_from_utf8(S, opname, (int)strlen(opname));

        ObjClosure *cl = find_overload(c->methods, key, argc, args);
        if (cl != NULL) return cl;
    }
    return NULL;
}

ObjClosure *class_find_method_any(ArtState *S, ObjClass *klass,
                                  ObjString *name)
{
    (void)S;
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->methods, name);
        if (IS_TABLE(v))
        {
            ObjTable *ov = AS_TABLE(v);
            if (ov->array_count > 0)
                return AS_CLOSURE(ov->array[0]);
        }
    }
    return NULL;
}

Value class_instantiate(ArtState *S, ObjClass *klass,
                        int argc, Value *args, Node *at)
{
    ObjInstance *inst = obj_instance_new(S, klass);
    GC_PUSH(S, OBJ_VAL(inst));

    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        for (int i = 0; i < c->field_count; i++)
        {
            Field *f = &c->fields[i];
            if (f->is_static) continue;
            table_set(S, inst->fields, f->name, f->default_value);
        }
    }

    ObjClosure *ctor = class_find_method(S, klass, klass->name,
                                         argc, args, at);
    if (ctor == NULL)
    {
        if (argc == 0)
        {
            inst->is_frozen = true;
            GC_POP(S, 1);
            return OBJ_VAL(inst);
        }
        GC_POP(S, 1);
        art_runtime_error(S, at, "no constructor for %s with %d args",
                          obj_string_to_utf8(klass->name), argc);
    }

    GC_PUSH(S, OBJ_VAL(ctor));
    call_method(S, ctor, OBJ_VAL(inst), argc, args, at);
    GC_POP(S, 1);

    inst->is_frozen = true;

    GC_POP(S, 1);
    return OBJ_VAL(inst);
}
