// ============================================================
// class_member.c — member access, this/super, toString
// ============================================================

#include "class.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "register.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Value get_this(ArtState *S)
{
    Value v;
    if (!art_scope_lookup(S->scope, S->this_name, NULL, &v))
        v = NIL_VAL;
    return v;
}

static ObjClosure *find_getter(ObjClass *klass, ObjString *name,
                               ObjClass **owner)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->getters, name);
        if (IS_CLOSURE(v))
        {
            if (owner)
                *owner = c;
            return AS_CLOSURE(v);
        }
    }
    return NULL;
}

static ObjClosure *find_setter(ObjClass *klass, ObjString *name,
                               ObjClass **owner)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->setters, name);
        if (IS_CLOSURE(v))
        {
            if (owner)
                *owner = c;
            return AS_CLOSURE(v);
        }
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

// Look up a method by name on klass (or a superclass) and
// return a bound method. Only reached from the reflection path
// where we know the member exists. `at` is used for the privacy
// error location.
static Value bind_method(ArtState *S, ObjInstance *inst,
                         ObjString *name, Node *at)
{
    for (ObjClass *c = inst->klass; c != NULL; c = c->superclass)
    {
        Value v = table_get(c->methods, name);
        if (IS_TABLE(v))
        {
            ObjTable *ov = AS_TABLE(v);
            if (ov->array_count > 0)
            {
                Value first = ov->array[0];
                if (IS_CLOSURE(first))
                {
                    ObjClosure *cl = AS_CLOSURE(first);
                    if (cl->function->is_private && S->active_class != c)
                        art_runtime_error(S, at, "method '%s' is private",
                                          obj_string_to_utf8(name));
                }
                ObjBoundMethod *bm = obj_bound_method_new(
                    S, OBJ_VAL(inst), first);
                bm->start_class = c;
                return OBJ_VAL(bm);
            }
        }
    }
    return NIL_VAL;
}

Value instance_get_member(ArtState *S, ObjInstance *inst,
                          ObjString *name, Node *at)
{
    // 1. Field.
    ObjClass *field_owner = NULL;
    Field *f = find_field(inst->klass, name, &field_owner);
    if (f != NULL)
    {
        if (f->is_private && S->active_class != field_owner)
            art_runtime_error(S, at, "field '%s' is private",
                              obj_string_to_utf8(name));
        return table_get(inst->fields, name);
    }

    // 2. Getter.
    ObjClass *getter_owner = NULL;
    ObjClosure *getter = find_getter(inst->klass, name, &getter_owner);
    if (getter != NULL)
    {
        if (getter->function->is_private && S->active_class != getter_owner)
            art_runtime_error(S, at, "getter '%s' is private",
                              obj_string_to_utf8(name));
        return call_method(S, getter, OBJ_VAL(inst), 0, NULL, at);
    }

    // 3. Method.
    Value bm = bind_method(S, inst, name, at);
    if (!IS_NIL(bm))
        return bm;

    art_runtime_error(S, at, "no member '%s' on %s",
                      obj_string_to_utf8(name),
                      obj_string_to_utf8(inst->klass->name));
    return NIL_VAL;
}

// Soft lookup used by implicit `this.` resolution in eval_var.
// Same order as instance_get_member — field, getter, method —
// but returns false instead of raising when nothing matches.
bool instance_try_get_member(ArtState *S, ObjInstance *inst,
                             ObjString *name, Value *out, Node *at)
{
    // 1. Field.
    ObjClass *field_owner = NULL;
    Field *f = find_field(inst->klass, name, &field_owner);
    if (f != NULL)
    {
        if (f->is_private && S->active_class != field_owner)
            art_runtime_error(S, at, "field '%s' is private",
                              obj_string_to_utf8(name));
        *out = table_get(inst->fields, name);
        return true;
    }

    // 2. Getter.
    ObjClass *getter_owner = NULL;
    ObjClosure *getter = find_getter(inst->klass, name, &getter_owner);
    if (getter != NULL)
    {
        if (getter->function->is_private && S->active_class != getter_owner)
            art_runtime_error(S, at, "getter '%s' is private",
                              obj_string_to_utf8(name));
        *out = call_method(S, getter, OBJ_VAL(inst), 0, NULL, at);
        return true;
    }

    // 3. Method.
    Value bm = bind_method(S, inst, name, at);
    if (!IS_NIL(bm))
    {
        *out = bm;
        return true;
    }

    return false;
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

    ObjClass *setter_owner = NULL;
    ObjClosure *setter = find_setter(inst->klass, name, &setter_owner);
    if (setter != NULL)
    {
        if (setter->function->is_private && S->active_class != setter_owner)
            art_runtime_error(S, at, "setter '%s' is private",
                              obj_string_to_utf8(name));
        Value args[1] = {v};
        return call_method(S, setter, OBJ_VAL(inst), 1, args, at);
    }

    art_runtime_error(S, at, "no member '%s' on %s",
                      obj_string_to_utf8(name),
                      obj_string_to_utf8(inst->klass->name));
    return NIL_VAL;
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

// ============================================================
// Reflection
// ============================================================

static Value instance_fields_method(ArtState *S, int argc, Value *argv);
static Value instance_get_method(ArtState *S, int argc, Value *argv);
static Value instance_set_method(ArtState *S, int argc, Value *argv);

static bool is_reflection_method(Value v)
{
    if (!IS_NATIVE(v))
        return false;
    NativeFn fn = AS_NATIVE(v)->fn;
    return fn == instance_fields_method ||
           fn == instance_get_method ||
           fn == instance_set_method;
}

static Value instance_fields_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    (void)argv;
    Value self = get_this(S);
    if (!IS_INSTANCE(self))
        return NIL_VAL;

    ObjInstance *inst = AS_INSTANCE(self);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    ObjTable *seen = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(seen));

    for (ObjClass *c = inst->klass; c != NULL; c = c->superclass)
    {
        // Fields. Statics live on the class, not the instance.
        for (int i = 0; i < c->field_count; i++)
        {
            Field *f = &c->fields[i];
            if (f->is_static || f->is_private)
                continue;
            if (table_has(seen, f->name))
                continue;
            table_set(S, seen, f->name, BOOL_VAL(true));
            table_push(S, out, OBJ_VAL(f->name));
        }

        // Getters. Always closures — no union handling needed.
        for (int i = 0; i < c->getters->hash_capacity; i++)
        {
            TableEntry *e = &c->getters->entries[i];
            if (e->key == NULL)
                continue;
            if (table_has(seen, e->key))
                continue;
            Value gv = e->value;
            if (IS_CLOSURE(gv) && AS_CLOSURE(gv)->function->is_private)
                continue;
            table_set(S, seen, e->key, BOOL_VAL(true));
            table_push(S, out, OBJ_VAL(e->key));
        }

        // Setters. Same reasoning.
        for (int i = 0; i < c->setters->hash_capacity; i++)
        {
            TableEntry *e = &c->setters->entries[i];
            if (e->key == NULL)
                continue;
            if (table_has(seen, e->key))
                continue;
            Value sv = e->value;
            if (IS_CLOSURE(sv) && AS_CLOSURE(sv)->function->is_private)
                continue;
            table_set(S, seen, e->key, BOOL_VAL(true));
            table_push(S, out, OBJ_VAL(e->key));
        }

        // Methods. The overload list can hold closures or
        // natives. Closures carry privacy, constructor, and
        // operator flags. Natives are always public — but the
        // reflection trio gets skipped anyway, since those are
        // the methods a script calls to walk the object, not
        // members the walk should report.
        for (int i = 0; i < c->methods->hash_capacity; i++)
        {
            TableEntry *e = &c->methods->entries[i];
            if (e->key == NULL)
                continue;
            if (table_has(seen, e->key))
                continue;

            Value mv = e->value;
            if (IS_TABLE(mv))
            {
                ObjTable *ov = AS_TABLE(mv);
                if (ov->array_count > 0)
                {
                    Value first = ov->array[0];
                    if (IS_CLOSURE(first))
                    {
                        ObjFunction *fn = AS_CLOSURE(first)->function;
                        if (fn->is_private)
                            continue;
                        if (fn->is_constructor)
                            continue;
                        if (fn->is_operator)
                            continue;
                    }
                    else if (is_reflection_method(first))
                    {
                        continue;
                    }
                }
            }
            table_set(S, seen, e->key, BOOL_VAL(true));
            table_push(S, out, OBJ_VAL(e->key));
        }
    }

    GC_POP(S, 2);
    return OBJ_VAL(out);
}

static Value instance_get_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_INSTANCE(self))
        return NIL_VAL;
    if (!IS_STRING(argv[0]))
        art_runtime_error(S, NULL, "get: key must be a string");

    ObjString *name = AS_STRING(argv[0]);
    Value out;
    if (!instance_try_get_member(S, AS_INSTANCE(self), name, &out, NULL))
        art_runtime_error(S, NULL, "no member '%s'",
                          obj_string_to_utf8(name));
    return out;
}

static Value instance_set_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_INSTANCE(self))
        return NIL_VAL;
    if (!IS_STRING(argv[0]))
        art_runtime_error(S, NULL, "set: key must be a string");

    ObjString *name = AS_STRING(argv[0]);
    return instance_set_member(S, AS_INSTANCE(self), name, argv[1], NULL);
}

void class_register_reflection(ArtState *S, ObjClass *klass)
{
    art_define_method(S, klass, "fields", instance_fields_method, 0);
    art_define_method(S, klass, "get", instance_get_method, 1);
    art_define_method(S, klass, "set", instance_set_method, 2);
}

// ============================================================
// toString dispatch
// ============================================================

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
