#include "enum.h"
#include "interp.h"
#include "scope.h"

#include <string.h>
#include <stdlib.h>

static void define_enum_native(ArtState *S, ObjEnum *e, const char *name,
                               NativeFn fn, int arity)
{
    ObjString *n = obj_string_from_utf8(S, name, (int)strlen(name));
    GC_PUSH(S, OBJ_VAL(n));
    ObjNative *nat = obj_native_new(S, fn, n, arity);
    GC_POP(S, 1);

    GC_PUSH(S, OBJ_VAL(nat));
    table_set(S, e->methods, n, OBJ_VAL(nat));
    GC_POP(S, 1);
}

static ObjEnum *this_enum(ArtState *S)
{
    ObjString *tn = obj_string_from_utf8(S, "this", 4);
    Value v;
    if (!art_scope_lookup(S->scope, tn, NULL, &v))
        return NULL;
    if (!IS_ENUM(v))
        return NULL;
    return AS_ENUM(v);
}

static Value enum_values(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));
    for (int i = 0; i < e->member_count; i++)
        table_push(S, out, OBJ_VAL(e->ordered[i]));
    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value enum_names(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));
    for (int i = 0; i < e->member_count; i++)
        table_push(S, out, OBJ_VAL(e->ordered[i]->name));
    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value enum_from_name(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;
    if (!IS_STRING(argv[0])) return NIL_VAL;
    if (!table_has(e->members, AS_STRING(argv[0]))) return NIL_VAL;
    return table_get(e->members, AS_STRING(argv[0]));
}

static Value enum_from_value(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    ObjEnum *e = this_enum(S);
    if (e == NULL) return NIL_VAL;
    for (int i = 0; i < e->member_count; i++)
    {
        if (value_equal(e->ordered[i]->value, argv[0]))
            return OBJ_VAL(e->ordered[i]);
    }
    return NIL_VAL;
}

Value eval_enum_decl(ArtState *S, Node *n)
{
    EnumDeclNode *d = (EnumDeclNode *)n;

    ObjEnum *e = obj_enum_new(S, d->name);
    GC_PUSH(S, OBJ_VAL(e));

    if (d->member_count > 0)
        e->ordered = malloc(sizeof(ObjEnumValue *) * d->member_count);

    Value first_val = NIL_VAL;
    bool have_type = false;

    for (int i = 0; i < d->member_count; i++)
    {
        EnumMemberNode *m = (EnumMemberNode *)d->members[i];

        Value v = NIL_VAL;
        if (m->value != NULL)
        {
            v = art_eval(S, m->value);
            if (S->control != CONTROL_NONE) { GC_POP(S, 1); return NIL_VAL; }

            if (!have_type)
            {
                first_val = v;
                have_type = true;
            }
            else if (v.type != first_val.type)
            {
                art_runtime_error(S, m->value,
                                  "enum member '%s' has type %s, expected %s",
                                  obj_string_to_utf8(m->name),
                                  value_type_name(v),
                                  value_type_name(first_val));
            }
        }

        ObjEnumValue *ev = obj_enum_value_new(S, e, m->name, v);
        GC_PUSH(S, OBJ_VAL(ev));

        table_set(S, e->members, m->name, OBJ_VAL(ev));
        e->ordered[e->member_count++] = ev;

        GC_POP(S, 1);
    }

    define_enum_native(S, e, "values", enum_values, 0);
    define_enum_native(S, e, "names", enum_names, 0);
    define_enum_native(S, e, "fromName", enum_from_name, 1);
    define_enum_native(S, e, "fromValue", enum_from_value, 1);

    art_scope_declare(S, S->scope, d->name, OBJ_VAL(e));

    GC_POP(S, 1);
    return OBJ_VAL(e);
}

Value enum_value_get_member(ArtState *S, ObjEnumValue *ev,
                            ObjString *name, Node *at)
{
    if (obj_string_eq_ascii(name, "name")) return OBJ_VAL(ev->name);
    if (obj_string_eq_ascii(name, "value")) return ev->value;

    art_runtime_error(S, at, "no member '%s' on enum value",
                      obj_string_to_utf8(name));
    return NIL_VAL;
}

Value enum_get_member(ArtState *S, ObjEnum *e, ObjString *name, Node *at)
{
    if (table_has(e->members, name))
        return table_get(e->members, name);

    if (table_has(e->methods, name))
    {
        Value v = table_get(e->methods, name);
        if (IS_NATIVE(v))
        {
            ObjBoundMethod *bm = obj_bound_method_new(S, OBJ_VAL(e), v);
            return OBJ_VAL(bm);
        }
        return v;
    }

    art_runtime_error(S, at, "no member '%s' on enum '%s'",
                      obj_string_to_utf8(name),
                      obj_string_to_utf8(e->name));
    return NIL_VAL;
}
