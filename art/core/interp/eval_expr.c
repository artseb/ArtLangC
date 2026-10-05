// ============================================================
// eval_expr.c — literals, variables, operators, interpolation
// ============================================================

#include "interp.h"
#include "scope.h"
#include "format.h"
#include "features/registry.h"

#include <math.h>
#include <string.h>

static bool value_in_string(ObjString *needle, ObjString *hay);
static bool value_in_table(Value v, ObjTable *t);

Value eval_literal(ArtState *S, Node *n)
{
    (void)S;
    return ((LiteralNode *)n)->value;
}

Value eval_var(ArtState *S, Node *n)
{
    VarNode *v = (VarNode *)n;
    Value out;
    if (art_scope_lookup(S->scope, v->name, NULL, &out))
        return out;

    // Undefined variables read as nil, matching Lua. But the most
    // common cause of an undefined read inside a method body is
    // forgetting `this.`: writing `name` when you meant
    // `this.name`. When we can tell that's what happened, say so
    // instead of silently returning nil.
    //
    // Outside a method — or when the name doesn't match any member
    // of the current class — we fall through to nil.
    ObjString *this_name = obj_string_from_utf8(S, "this", 4);
    Value this_val;
    if (art_scope_lookup(S->scope, this_name, NULL, &this_val) &&
        IS_INSTANCE(this_val))
    {
        ObjInstance *inst = AS_INSTANCE(this_val);
        for (ObjClass *c = inst->klass; c != NULL; c = c->superclass)
        {
            for (int i = 0; i < c->field_count; i++)
            {
                if (c->fields[i].name == v->name)
                {
                    art_runtime_error(S, n,
                                      "undefined variable '%s' in method body "
                                      "(did you mean 'this.%s'?)",
                                      obj_string_to_utf8(v->name),
                                      obj_string_to_utf8(v->name));
                }
            }

            if (table_has(c->methods, v->name) ||
                table_has(c->getters, v->name))
            {
                art_runtime_error(S, n,
                                  "undefined variable '%s' in method body "
                                  "(did you mean 'this.%s'?)",
                                  obj_string_to_utf8(v->name),
                                  obj_string_to_utf8(v->name));
            }
        }
    }

    return NIL_VAL;
}

Value eval_unary(ArtState *S, Node *n)
{
    UnaryNode *u = (UnaryNode *)n;
    Value v = art_eval(S, u->operand);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (u->op == TOKEN_BANG)
        return BOOL_VAL(value_is_falsy(v));

    if (u->op == TOKEN_MINUS)
    {
        if (IS_INT(v))
            return INT_VAL(-AS_INT(v));
        if (IS_FLOAT(v))
            return FLOAT_VAL(-AS_FLOAT(v));

        FEATURES_FOR_EACH(f)
        {
            if (f->unop == NULL)
                continue;
            Value out;
            if (f->unop(S, n, u->op, v, &out))
                return out;
        }

        art_runtime_error(S, n, "cannot negate %s", value_type_name(v));
    }

    art_runtime_error(S, n, "unknown unary operator");
    return NIL_VAL;
}

Value eval_binop(ArtState *S, Node *at, TokenType op, Value a, Value b)
{
    if (op == TOKEN_PLUS && IS_STRING(a) && IS_STRING(b))
    {
        ObjString *r = obj_string_concat(S, AS_STRING(a), AS_STRING(b));
        return OBJ_VAL(r);
    }

    if (op == TOKEN_IN)
    {
        if (IS_TABLE(b))
            return BOOL_VAL(value_in_table(a, AS_TABLE(b)));
        if (IS_STRING(a) && IS_STRING(b))
            return BOOL_VAL(value_in_string(AS_STRING(a), AS_STRING(b)));
    }

    FEATURES_FOR_EACH(f)
    {
        if (f->binop == NULL)
            continue;
        Value out;
        if (f->binop(S, at, op, a, b, &out))
            return out;
    }

    if (op == TOKEN_EQUAL_EQUAL)
        return BOOL_VAL(value_equal(a, b));
    if (op == TOKEN_BANG_EQUAL)
        return BOOL_VAL(!value_equal(a, b));

    if ((op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
         op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL) &&
        IS_STRING(a) && IS_STRING(b))
    {
        ObjString *sa = AS_STRING(a);
        ObjString *sb = AS_STRING(b);
        int m = sa->unit_count < sb->unit_count
                    ? sa->unit_count
                    : sb->unit_count;
        int cmp = 0;
        for (int i = 0; i < m; i++)
        {
            if (sa->chars[i] != sb->chars[i])
            {
                cmp = sa->chars[i] < sb->chars[i] ? -1 : 1;
                break;
            }
        }
        if (cmp == 0)
            cmp = (sa->unit_count > sb->unit_count) -
                  (sa->unit_count < sb->unit_count);

        switch (op)
        {
        case TOKEN_LESS:
            return BOOL_VAL(cmp < 0);
        case TOKEN_LESS_EQUAL:
            return BOOL_VAL(cmp <= 0);
        case TOKEN_GREATER:
            return BOOL_VAL(cmp > 0);
        case TOKEN_GREATER_EQUAL:
            return BOOL_VAL(cmp >= 0);
        default:
            break;
        }
    }

    if (op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
        op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL)
    {
        art_runtime_error(S, at, "comparison on %s and %s",
                          value_type_name(a), value_type_name(b));
    }

    if (op == TOKEN_IN)
        art_runtime_error(S, at, "'in' not supported between %s and %s",
                          value_type_name(a), value_type_name(b));

    art_runtime_error(S, at, "arithmetic on %s and %s",
                      value_type_name(a), value_type_name(b));
    return NIL_VAL;
}

Value eval_binary(ArtState *S, Node *n)
{
    BinaryNode *b = (BinaryNode *)n;

    if (b->op == TOKEN_AND)
    {
        Value a = art_eval(S, b->left);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
        if (value_is_falsy(a))
            return a;
        return art_eval(S, b->right);
    }
    if (b->op == TOKEN_OR)
    {
        Value a = art_eval(S, b->left);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
        if (!value_is_falsy(a))
            return a;
        return art_eval(S, b->right);
    }

    Value a = art_eval(S, b->left);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;
    Value r = art_eval(S, b->right);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    return eval_binop(S, n, b->op, a, r);
}

Value eval_interp(ArtState *S, Node *n)
{
    InterpNode *in = (InterpNode *)n;

    ObjString *result = NULL;

    for (int i = 0; i < in->part_count; i++)
    {
        Value v = art_eval(S, in->parts[i]);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;

        ObjString *s;
        if (in->specs != NULL && in->specs[i] != NULL)
            s = format_value(S, v, in->specs[i]);
        else
            s = value_to_string(S, v);

        // format_value can raise on a bad spec; the control
        // flag is the signal that we should unwind.
        if (S->control != CONTROL_NONE)
            return NIL_VAL;

        if (result == NULL)
            result = s;
        else
            result = obj_string_concat(S, result, s);
    }

    if (result == NULL)
        result = obj_string_from_utf8(S, "", 0);

    return OBJ_VAL(result);
}

Value eval_is(ArtState *S, Node *n)
{
    IsNode *in = (IsNode *)n;

    Value v = art_eval(S, in->left);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    ObjClass *type_class = NULL;
    ObjInterface *type_interface = NULL;

    Value tv;
    if (art_scope_lookup(S->scope, in->type_name, NULL, &tv))
    {
        if (IS_CLASS(tv))
            type_class = AS_CLASS(tv);
        else if (IS_INTERFACE(tv))
            type_interface = AS_INTERFACE(tv);
    }

    if (type_class == NULL && type_interface == NULL &&
        !value_type_name_is_builtin(in->type_name))
    {
        art_runtime_error(S, n, "unknown type '%s'",
                          obj_string_to_utf8(in->type_name));
    }

    return BOOL_VAL(value_matches_type(v, in->type_name,
                                       type_class, type_interface));
}

static bool value_in_string(ObjString *needle, ObjString *hay)
{
    if (needle->unit_count == 0)
        return true;
    if (needle->unit_count > hay->unit_count)
        return false;

    int limit = hay->unit_count - needle->unit_count;
    for (int i = 0; i <= limit; i++)
    {
        if (memcmp(hay->chars + i, needle->chars,
                   sizeof(uint16_t) * needle->unit_count) == 0)
            return true;
    }
    return false;
}

static bool value_in_table(Value v, ObjTable *t)
{
    for (int i = 0; i < t->array_count; i++)
        if (value_equal(t->array[i], v))
            return true;

    if (IS_STRING(v))
        return table_has(t, AS_STRING(v));

    return false;
}
