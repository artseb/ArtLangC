// ============================================================
// class_member.c — member access, this/super, toString
// ============================================================

#include "class.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ObjClosure *find_getter(ObjClass *klass, ObjString *name)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->getters, name);
        if (IS_CLOSURE(v))
            return AS_CLOSURE(v);
    }
    return NULL;
}

static ObjClosure *find_setter(ObjClass *klass, ObjString *name)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->setters, name);
        if (IS_CLOSURE(v))
            return AS_CLOSURE(v);
    }
    return NULL;
}

static Field *find_field(ObjClass *klass, ObjString *name, ObjClass **owner)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        for (int i = 0; i < c->field_count; i++)
        {
            if (c->fields[i].name == name)
            {
                if (owner)
                    *owner = c;
                return &c->fields[i];
            }
        }
    }
    return NULL;
}

Value instance_get_member(ArtState *S, ObjInstance *inst,
                          ObjString *name, Node *at)
{
    ObjClass *owner = NULL;
    Field *f = find_field(inst->klass, name, &owner);
    if (f != NULL)
    {
        if (f->is_private && S->active_class != owner)
            art_runtime_error(S, at, "field '%s' is private",
                              obj_string_to_utf8(name));
        return table_get(inst->fields, name);
    }

    ObjClosure *getter = find_getter(inst->klass, name);
    if (getter != NULL)
        return call_method(S, getter, OBJ_VAL(inst), 0, NULL, at);

    for (ObjClass *c = inst->klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->methods, name);
        if (IS_TABLE(v))
        {
            ObjTable *ov = AS_TABLE(v);
            if (ov->array_count > 0)
            {
                ObjClosure *cl = AS_CLOSURE(ov->array[0]);
                ObjBoundMethod *bm = obj_bound_method_new(S, OBJ_VAL(inst), OBJ_VAL(cl));
                bm->start_class = c;
                return OBJ_VAL(bm);
            }
        }
    }

    art_runtime_error(S, at, "no member '%s' on %s",
                      obj_string_to_utf8(name),
                      obj_string_to_utf8(inst->klass->name));
    return NIL_VAL;
}

Value instance_set_member(ArtState *S, ObjInstance *inst,
                          ObjString *name, Value v, Node *at)
{
    ObjClass *owner = NULL;
    Field *f = find_field(inst->klass, name, &owner);
    if (f != NULL)
    {
        if (f->is_private && S->active_class != owner)
            art_runtime_error(S, at, "field '%s' is private",
                              obj_string_to_utf8(name));
        if (inst->is_frozen && f->is_const)
            art_runtime_error(S, at, "field '%s' is const",
                              obj_string_to_utf8(name));
        table_set(S, inst->fields, name, v);
        return v;
    }

    ObjClosure *setter = find_setter(inst->klass, name);
    if (setter != NULL)
    {
        Value args[1] = {v};
        return call_method(S, setter, OBJ_VAL(inst), 1, args, at);
    }

    art_runtime_error(S, at, "no member '%s' on %s",
                      obj_string_to_utf8(name),
                      obj_string_to_utf8(inst->klass->name));
    return NIL_VAL;
}

bool instance_try_get_member(ArtState *S, ObjInstance *inst,
                             ObjString *name, Value *out, Node *at)
{
    // 1. Field
    ObjClass *owner = NULL;
    Field *f = find_field(inst->klass, name, &owner);
    if (f != NULL)
    {
        if (f->is_private && S->active_class != owner)
            art_runtime_error(S, at, "field '%s' is private",
                              obj_string_to_utf8(name));
        *out = table_get(inst->fields, name);
        return true;
    }

    // 2. Getter
    ObjClosure *getter = find_getter(inst->klass, name);
    if (getter != NULL)
    {
        *out = call_method(S, getter, OBJ_VAL(inst), 0, NULL, at);
        return true;
    }

    // 3. Method — return a bound method. Overload is picked at
    //    call time, not here.
    for (ObjClass *c = inst->klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->methods, name);
        if (IS_TABLE(v))
        {
            ObjTable *ov = AS_TABLE(v);
            if (ov->array_count > 0)
            {
                ObjClosure *cl = AS_CLOSURE(ov->array[0]);
                ObjBoundMethod *bm = obj_bound_method_new(
                    S, OBJ_VAL(inst), OBJ_VAL(cl));
                bm->start_class = c;
                *out = OBJ_VAL(bm);
                return true;
            }
        }
    }

    return false;
}

Value interp_this(ArtState *S, Node *at)
{
    Value v;
    if (!art_scope_lookup(S->scope, S->this_name, NULL, &v))
        art_runtime_error(S, at, "'this' outside of a method");
    return v;
}

Value eval_this(ArtState *S, Node *n)
{
    return interp_this(S, n);
}

Value eval_super(ArtState *S, Node *n)
{
    art_runtime_error(S, n, "'super' must be called or indexed");
    return NIL_VAL;
}

ObjClass *builtin_class_of(ArtState *S, Value v)
{
    if (IS_STRING(v))
        return S->builtin_classes[OBJ_STRING];
    if (IS_TABLE(v))
        return S->builtin_classes[OBJ_TABLE];
    return NULL;
}

static bool class_to_string_impl(ArtState *S, Value v, ObjString **out,
                                 int depth)
{
    if (!IS_INSTANCE(v))
        return false;
    ObjInstance *inst = AS_INSTANCE(v);

    if (depth > 8)
    {
        *out = obj_string_from_utf8(S, "<recursion>", 11);
        return true;
    }

    ObjString *name = obj_string_from_utf8(S, "toString", 8);
    ObjClosure *ts = class_find_method_any(S, inst->klass, name);

    if (ts == NULL)
    {
        char *cn = obj_string_to_utf8(inst->klass->name);
        *out = obj_string_from_fmt(S, "<%s instance>", cn);
        free(cn);
        return true;
    }

    Value r = call_method(S, ts, v, 0, NULL, NULL);

    if (IS_STRING(r))
    {
        *out = AS_STRING(r);
        return true;
    }

    if (IS_INSTANCE(r))
        return class_to_string_impl(S, r, out, depth + 1);

    *out = value_to_string(S, r);
    return true;
}

bool class_to_string(ArtState *S, Value v, ObjString **out)
{
    return class_to_string_impl(S, v, out, 0);
}
