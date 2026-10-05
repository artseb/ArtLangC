#include "interp.h"
#include "scope.h"
#include "features/registry.h"

Value eval_table_literal(ArtState *S, Node *n)
{
    TableNode *tn = (TableNode *)n;
    ObjTable *t = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(t));

    for (int i = 0; i < tn->array_count; i++)
    {
        Value v = art_eval(S, tn->array_items[i]);
        if (S->control != CONTROL_NONE)
        {
            GC_POP(S, 1);
            return NIL_VAL;
        }
        GC_PUSH(S, v);
        table_push(S, t, v);
        GC_POP(S, 1);
    }

    for (int i = 0; i < tn->hash_count; i++)
    {
        Value v = art_eval(S, tn->hash_values[i]);
        if (S->control != CONTROL_NONE)
        {
            GC_POP(S, 1);
            return NIL_VAL;
        }
        GC_PUSH(S, v);
        table_set(S, t, tn->hash_keys[i], v);
        GC_POP(S, 1);
    }

    GC_POP(S, 1);
    return OBJ_VAL(t);
}

// Resolve a user-supplied int index into a 1-based array slot.
// Negative indices count from the end: -1 is the last element,
// -2 the second-to-last, and so on. Returns 0 if out of range.
static int64_t resolve_index(ObjTable *t, int64_t i)
{
    if (i < 0)
        i = t->array_count + 1 + i;
    if (i < 1 || i > t->array_count)
        return 0;
    return i;
}

static Value table_index_get(ArtState *S, ObjTable *t, Value idx, Node *at)
{
    if (IS_INT(idx))
    {
        int64_t i = resolve_index(t, AS_INT(idx));
        if (i == 0)
            return NIL_VAL;
        return t->array[i - 1];
    }
    if (IS_STRING(idx))
        return table_get(t, AS_STRING(idx));

    art_runtime_error(S, at, "table index must be int or string, got %s",
                      value_type_name(idx));
    return NIL_VAL;
}

static void table_index_set(ArtState *S, ObjTable *t, Value idx,
                            Value v, Node *at)
{
    if (IS_INT(idx))
    {
        int64_t raw = AS_INT(idx);
        int64_t i = raw;
        if (i < 0)
            i = t->array_count + 1 + i;

        if (i < 1)
            art_runtime_error(S, at, "table index out of range (got %lld)",
                              (long long)raw);

        if (i <= t->array_count)
        {
            t->array[i - 1] = v;
            return;
        }

        while (t->array_count < i - 1)
            table_push(S, t, NIL_VAL);
        table_push(S, t, v);
        return;
    }
    if (IS_STRING(idx))
    {
        table_set(S, t, AS_STRING(idx), v);
        return;
    }

    art_runtime_error(S, at, "table index must be int or string, got %s",
                      value_type_name(idx));
}

Value eval_index(ArtState *S, Node *n)
{
    IndexNode *ix = (IndexNode *)n;

    Value target = art_eval(S, ix->target);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;
    Value idx = art_eval(S, ix->index);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (!IS_TABLE(target))
        art_runtime_error(S, n, "cannot index %s with %s",
                          value_type_name(target),
                          value_type_name(idx));

    return table_index_get(S, AS_TABLE(target), idx, n);
}

Value eval_member(ArtState *S, Node *n)
{
    MemberNode *m = (MemberNode *)n;

    FEATURES_FOR_EACH(f)
    {
        if (f->pre_member_get == NULL)
            continue;
        Value out;
        if (f->pre_member_get(S, m, n, &out))
            return out;
    }

    Value target = art_eval(S, m->target);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    FEATURES_FOR_EACH(f)
    {
        if (f->member_get == NULL)
            continue;
        Value out;
        if (f->member_get(S, target, m->name, n, &out))
            return out;
    }

    if (IS_STRING(target) || IS_TABLE(target))
    {
        if (IS_TABLE(target))
        {
            Value stored = table_get(AS_TABLE(target), m->name);
            if (!IS_NIL(stored))
                return stored;
        }

        ObjClass *klass = builtin_class_of(S, target);
        if (klass != NULL)
        {
            Value v = table_get(klass->methods, m->name);
            ObjClosure *cl = NULL;
            ObjNative *nat = NULL;

            if (IS_TABLE(v))
            {
                ObjTable *ov = AS_TABLE(v);
                if (ov->array_count > 0)
                {
                    Value first = ov->array[0];
                    if (IS_CLOSURE(first))
                        cl = AS_CLOSURE(first);
                    else if (IS_NATIVE(first))
                        nat = AS_NATIVE(first);
                }
            }
            else if (IS_CLOSURE(v))
                cl = AS_CLOSURE(v);
            else if (IS_NATIVE(v))
                nat = AS_NATIVE(v);

            if (cl != NULL || nat != NULL)
            {
                Value method_val = cl ? OBJ_VAL(cl) : OBJ_VAL(nat);
                ObjBoundMethod *bm = obj_bound_method_new(S, target, method_val);
                bm->start_class = klass;
                return OBJ_VAL(bm);
            }
        }

        if (IS_STRING(target))
            art_runtime_error(S, n, "no method '%s' on string",
                              obj_string_to_utf8(m->name));
    }

    if (!IS_TABLE(target))
        art_runtime_error(S, n, "cannot access member '%s' on %s",
                          obj_string_to_utf8(m->name),
                          value_type_name(target));
    return table_get(AS_TABLE(target), m->name);
}

Value eval_assign_member(ArtState *S, Node *n, TokenType binop)
{
    AssignNode *a = (AssignNode *)n;
    MemberNode *m = (MemberNode *)a->target;

    Value target = art_eval(S, m->target);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    Value rhs;
    if (binop == TOKEN_EQUAL)
    {
        rhs = art_eval(S, a->value);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
    }
    else
    {
        Value cur;
        bool got_cur = false;

        FEATURES_FOR_EACH(f)
        {
            if (got_cur)
                continue;
            if (f->member_get == NULL)
                continue;
            if (f->member_get(S, target, m->name, a->target, &cur))
                got_cur = true;
        }

        if (!got_cur)
        {
            if (!IS_TABLE(target))
                art_runtime_error(S, a->target,
                                  "cannot assign member '%s' on %s",
                                  obj_string_to_utf8(m->name),
                                  value_type_name(target));
            cur = table_get(AS_TABLE(target), m->name);
        }

        Value r = art_eval(S, a->value);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
        rhs = eval_binop(S, n, binop, cur, r);
    }

    Value out;
    bool stored = false;

    FEATURES_FOR_EACH(f)
    {
        if (stored)
            continue;
        if (f->member_set == NULL)
            continue;
        if (f->member_set(S, target, m->name, rhs, a->target, &out))
            stored = true;
    }

    if (stored)
        return out;

    if (!IS_TABLE(target))
        art_runtime_error(S, a->target, "cannot assign member '%s' on %s",
                          obj_string_to_utf8(m->name),
                          value_type_name(target));

    if (AS_TABLE(target)->frozen)
        art_runtime_error(S, a->target, "cannot modify frozen table");

    table_set(S, AS_TABLE(target), m->name, rhs);
    return rhs;
}

Value eval_assign_index(ArtState *S, Node *n, TokenType binop)
{
    AssignNode *a = (AssignNode *)n;
    IndexNode *ix = (IndexNode *)a->target;

    Value target = art_eval(S, ix->target);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;
    Value idx = art_eval(S, ix->index);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (!IS_TABLE(target))
        art_runtime_error(S, a->target,
                          "cannot index-assign on %s with %s",
                          value_type_name(target),
                          value_type_name(idx));

    ObjTable *t = AS_TABLE(target);
    Value rhs;

    if (binop == TOKEN_EQUAL)
    {
        rhs = art_eval(S, a->value);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
    }
    else
    {
        Value cur = table_index_get(S, t, idx, a->target);
        Value r = art_eval(S, a->value);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
        rhs = eval_binop(S, n, binop, cur, r);
    }

    table_index_set(S, t, idx, rhs, a->target);
    return rhs;
}

static int run_for_in_iteration(ArtState *S, ForInNode *f,
                                Value key, Value value)
{
    art_scope_push(S);

    if (f->key != NULL)
    {
        DeclNode *kd = (DeclNode *)f->key;
        art_scope_declare(S, S->scope, kd->name, key);
    }
    DeclNode *vd = (DeclNode *)f->value;
    art_scope_declare(S, S->scope, vd->name, value);

    art_eval(S, f->body);

    int rc = 0;
    if (S->control == CONTROL_BREAK)
    {
        S->control = CONTROL_NONE;
        rc = 1;
    }
    else if (S->control == CONTROL_CONTINUE)
    {
        S->control = CONTROL_NONE;
    }
    else if (S->control == CONTROL_RETURN)
    {
        rc = 2;
    }

    art_scope_pop(S);
    return rc;
}

Value eval_for_in(ArtState *S, Node *n)
{
    ForInNode *f = (ForInNode *)n;

    Value iter = art_eval(S, f->iterable);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (!IS_TABLE(iter))
        art_runtime_error(S, n, "cannot iterate over %s",
                          value_type_name(iter));

    ObjTable *t = AS_TABLE(iter);
    int arr_n = t->array_count;

    for (int i = 0; i < arr_n; i++)
    {
        if (i >= t->array_count)
            break;
        Value v = t->array[i];
        if (run_for_in_iteration(S, f, INT_VAL(i + 1), v))
            return NIL_VAL;
    }

    for (int i = 0; i < t->hash_capacity; i++)
    {
        if (i >= t->hash_capacity)
            break;
        TableEntry *e = &t->entries[i];
        if (e->key == NULL)
            continue;
        Value k = OBJ_VAL(e->key);
        Value v = e->value;
        if (run_for_in_iteration(S, f, k, v))
            return NIL_VAL;
    }

    return NIL_VAL;
}
