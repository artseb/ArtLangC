// ============================================================
// eval_control.c — declarations, assignment, control flow
//
// eval_if works for both the statement form (branches are
// BLOCK nodes; the returned value is discarded by the caller)
// and the expression form (branches are expressions; the value
// is used).
//
// eval_multi_decl dispatches on the DECL_ASSIGN flag: with it,
// names are reassigned through the scope chain; without it,
// they are freshly declared.
// ============================================================

#include "interp.h"
#include "scope.h"
#include "features/registry.h"

Value eval_decl(ArtState *S, Node *n)
{
    DeclNode *d = (DeclNode *)n;

    Value v = d->value ? art_eval(S, d->value) : NIL_VAL;
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (d->flags & DECL_LOCAL)
    {
        if (d->flags & DECL_CONST)
            art_scope_declare_const(S, S->scope, d->name, v);
        else
            art_scope_declare(S, S->scope, d->name, v);
    }
    else
    {
        table_set(S, S->global_scope->vars, d->name, v);
        if (d->flags & DECL_CONST)
            table_set(S, S->global_scope->consts, d->name, BOOL_VAL(true));
    }
    return v;
}

static TokenType strip_compound(TokenType op)
{
    switch (op)
    {
    case TOKEN_PLUS_EQUAL: return TOKEN_PLUS;
    case TOKEN_MINUS_EQUAL: return TOKEN_MINUS;
    case TOKEN_STAR_EQUAL: return TOKEN_STAR;
    case TOKEN_SLASH_EQUAL: return TOKEN_SLASH;
    case TOKEN_PERCENT_EQUAL: return TOKEN_PERCENT;
    case TOKEN_CARET_EQUAL: return TOKEN_CARET;
    default: return TOKEN_EOF;
    }
}

Value eval_assign(ArtState *S, Node *n)
{
    AssignNode *a = (AssignNode *)n;

    TokenType binop = (a->op == TOKEN_EQUAL)
                          ? TOKEN_EQUAL
                          : strip_compound(a->op);

    if (a->target->type == NODE_MEMBER)
        return eval_assign_member(S, n, binop);
    if (a->target->type == NODE_INDEX)
        return eval_assign_index(S, n, binop);

    if (a->target->type != NODE_VAR)
        art_runtime_error(S, n, "invalid assignment target");

    VarNode *v = (VarNode *)a->target;
    Value rhs;

    if (binop == TOKEN_EQUAL)
    {
        rhs = art_eval(S, a->value);
    }
    else
    {
        Value cur;
        if (!art_scope_lookup(S->scope, v->name, NULL, &cur))
            cur = NIL_VAL;

        Value r = art_eval(S, a->value);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;

        rhs = eval_binop(S, n, binop, cur, r);
    }

    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (!art_scope_assign(S, S->scope, v->name, rhs, true))
        art_runtime_error(S, a->target, "'%s' is const",
                          obj_string_to_utf8(v->name));
    return rhs;
}

Value eval_block(ArtState *S, Node *n)
{
    BlockNode *b = (BlockNode *)n;

    art_scope_push(S);
    Value last = NIL_VAL;
    for (int i = 0; i < b->count; i++)
    {
        last = art_eval(S, b->stmts[i]);
        if (S->control != CONTROL_NONE)
            break;
    }
    art_scope_pop(S);
    return last;
}

// Returns the value of whichever branch ran. Statement-position
// callers discard it. Expression-position callers use it — the
// whole point of `local x = if (c) -> a else b`.
Value eval_if(ArtState *S, Node *n)
{
    IfNode *f = (IfNode *)n;

    Value c = art_eval(S, f->cond);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (value_is_falsy(c))
    {
        if (f->else_branch)
            return art_eval(S, f->else_branch);
        return NIL_VAL;
    }
    return art_eval(S, f->then_branch);
}

Value eval_while(ArtState *S, Node *n)
{
    WhileNode *w = (WhileNode *)n;

    for (;;)
    {
        Value c = art_eval(S, w->cond);
        if (S->control != CONTROL_NONE)
            return NIL_VAL;
        if (value_is_falsy(c))
            break;

        art_eval(S, w->body);

        if (S->control == CONTROL_BREAK)
        {
            S->control = CONTROL_NONE;
            break;
        }
        if (S->control == CONTROL_CONTINUE)
        {
            S->control = CONTROL_NONE;
            continue;
        }
        if (S->control == CONTROL_RETURN)
            return NIL_VAL;
    }
    return NIL_VAL;
}

Value eval_for_range(ArtState *S, Node *n)
{
    ForRangeNode *f = (ForRangeNode *)n;

    ObjString *var_name = NULL;
    Node *start_expr = NULL;

    if (f->init->type == NODE_DECL)
    {
        DeclNode *d = (DeclNode *)f->init;
        var_name = d->name;
        start_expr = d->value;
    }
    else if (f->init->type == NODE_ASSIGN)
    {
        AssignNode *a = (AssignNode *)f->init;
        if (a->target->type == NODE_VAR)
            var_name = ((VarNode *)a->target)->name;
        start_expr = a->value;
    }
    if (var_name == NULL)
        art_runtime_error(S, n, "for-range requires a variable");

    Value start_v = start_expr ? art_eval(S, start_expr) : NIL_VAL;
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    Value end_v = art_eval(S, f->end);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    Value step_v = f->step ? art_eval(S, f->step) : INT_VAL(1);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (!IS_NUMBER(start_v) || !IS_NUMBER(end_v) || !IS_NUMBER(step_v))
        art_runtime_error(S, n, "for-range bounds must be numbers");

    double d_end = AS_NUMBER(end_v);
    double d_step = AS_NUMBER(step_v);

    Value cur = start_v;

    for (;;)
    {
        double d_cur = AS_NUMBER(cur);
        if (d_step > 0 ? (d_cur > d_end) : (d_cur < d_end))
            break;

        art_scope_push(S);
        art_scope_declare(S, S->scope, var_name, cur);

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

        if (rc == 1)
            break;
        if (rc == 2)
            return NIL_VAL;

        if (IS_INT(cur) && IS_INT(step_v))
            cur = INT_VAL(AS_INT(cur) + AS_INT(step_v));
        else
            cur = FLOAT_VAL(d_cur + d_step);
    }

    return NIL_VAL;
}

Value eval_return(ArtState *S, Node *n)
{
    ReturnNode *r = (ReturnNode *)n;
    Value v = r->value ? art_eval(S, r->value) : NIL_VAL;
    S->return_value = v;
    S->control = CONTROL_RETURN;
    return v;
}

Value eval_break(ArtState *S, Node *n)
{
    (void)n;
    S->control = CONTROL_BREAK;
    return NIL_VAL;
}

Value eval_continue(ArtState *S, Node *n)
{
    (void)n;
    S->control = CONTROL_CONTINUE;
    return NIL_VAL;
}

// Two shapes share this node:
//   local a, b, c = expr     (flags & DECL_LOCAL, maybe DECL_CONST)
//   a, b, c = expr           (flags & DECL_ASSIGN)
//
// The value is destructured through the table's array part.
// Missing slots become nil; extra slots are ignored.
Value eval_multi_decl(ArtState *S, Node *n)
{
    MultiDeclNode *d = (MultiDeclNode *)n;

    Value v = art_eval(S, d->value);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    if (!IS_TABLE(v))
        art_runtime_error(S, n, "cannot destructure %s",
                          value_type_name(v));

    ObjTable *t = AS_TABLE(v);

    for (int i = 0; i < d->name_count; i++)
    {
        Value slot = (i < t->array_count) ? t->array[i] : NIL_VAL;

        if (d->flags & DECL_ASSIGN)
        {
            // Reassignment. Walks the scope chain; creates a global
            // if the name is unknown, matching bare-name assignment.
            if (!art_scope_assign(S, S->scope, d->names[i], slot, true))
                art_runtime_error(S, n, "'%s' is const",
                                  obj_string_to_utf8(d->names[i]));
        }
        else if (d->flags & DECL_LOCAL)
        {
            if (d->flags & DECL_CONST)
                art_scope_declare_const(S, S->scope, d->names[i], slot);
            else
                art_scope_declare(S, S->scope, d->names[i], slot);
        }
        else
        {
            table_set(S, S->global_scope->vars, d->names[i], slot);
        }
    }

    return v;
}
