// ============================================================
// interp.c — eval dispatch loop + embed entry points
// ============================================================

#include "interp.h"
#include "scope.h"
#include "parser.h"
#include "features/features.h"

#include <stdio.h>
#include <string.h>

typedef Value (*EvalFn)(ArtState *S, Node *n);

Value eval_literal(ArtState *S, Node *n);
Value eval_var(ArtState *S, Node *n);
Value eval_binary(ArtState *S, Node *n);
Value eval_unary(ArtState *S, Node *n);
Value eval_interp(ArtState *S, Node *n);
Value eval_is(ArtState *S, Node *n);

Value eval_decl(ArtState *S, Node *n);
Value eval_assign(ArtState *S, Node *n);
Value eval_block(ArtState *S, Node *n);
Value eval_if(ArtState *S, Node *n);
Value eval_while(ArtState *S, Node *n);
Value eval_for_range(ArtState *S, Node *n);
Value eval_return(ArtState *S, Node *n);
Value eval_break(ArtState *S, Node *n);
Value eval_continue(ArtState *S, Node *n);
Value eval_multi_decl(ArtState *S, Node *n);

Value eval_call(ArtState *S, Node *n);
Value eval_fun_decl(ArtState *S, Node *n);

Value eval_table_literal(ArtState *S, Node *n);
Value eval_index(ArtState *S, Node *n);
Value eval_member(ArtState *S, Node *n);
Value eval_for_in(ArtState *S, Node *n);

static EvalFn const eval_table[NODE_TYPE_COUNT] = {
    [NODE_LITERAL] = eval_literal,
    [NODE_VAR] = eval_var,
    [NODE_BINARY] = eval_binary,
    [NODE_UNARY] = eval_unary,
    [NODE_CALL] = eval_call,
    [NODE_DECL] = eval_decl,
    [NODE_ASSIGN] = eval_assign,
    [NODE_BLOCK] = eval_block,
    [NODE_IF] = eval_if,
    [NODE_WHILE] = eval_while,
    [NODE_FOR_RANGE] = eval_for_range,
    [NODE_FOR_IN] = eval_for_in,
    [NODE_RETURN] = eval_return,
    [NODE_BREAK] = eval_break,
    [NODE_CONTINUE] = eval_continue,
    [NODE_FUN_DECL] = eval_fun_decl,
    [NODE_TABLE] = eval_table_literal,
    [NODE_INDEX] = eval_index,
    [NODE_MEMBER] = eval_member,
    [NODE_INTERP] = eval_interp,
    [NODE_MULTI_DECL] = eval_multi_decl,
    [NODE_IS] = eval_is,
};

static Value eval_unimplemented(ArtState *S, Node *n)
{
    art_runtime_error(S, n, "interpreter: %s not yet implemented",
                      node_type_name(n->type));
    return NIL_VAL;
}

Value art_eval(ArtState *S, Node *n)
{
    if (n == NULL)
        return NIL_VAL;

    FEATURES_FOR_EACH(f)
    {
        if (f->eval_node == NULL)
            continue;
        Value out;
        if (f->eval_node(S, n, &out))
            return out;
    }

    EvalFn fn = (n->type < NODE_TYPE_COUNT) ? eval_table[n->type] : NULL;
    if (fn == NULL)
        return eval_unimplemented(S, n);

    return fn(S, n);
}

void interp_init(ArtState *S)
{
    (void)S;
}

Value art_run_ast(ArtState *S, Node *program)
{
    if (program == NULL)
        return NIL_VAL;

    if (program->type == NODE_BLOCK)
    {
        BlockNode *b = (BlockNode *)program;
        Value last = NIL_VAL;
        for (int i = 0; i < b->count; i++)
        {
            last = art_eval(S, b->stmts[i]);
            if (S->control != CONTROL_NONE)
                break;
        }

        if (S->control == CONTROL_RETURN)
            return S->return_value;

        return last;
    }

    return art_eval(S, program);
}

Value art_run_source(ArtState *S, const char *source, int length,
                     const char *file_name)
{
    S->last_error = false;

    const char *saved_file = S->current_file;
    S->current_file = file_name;

    Node *program = parse_source(S, source, length, file_name);
    if (program == NULL)
    {
        S->last_error = true;
        S->current_file = saved_file;
        return NIL_VAL;
    }

    ErrorFrame frame;
    ErrorFrame *volatile fp = &frame;
    fp->prev = S->error_frame;
    fp->file_name = file_name;
    fp->suppress_output = false;
    fp->message[0] = '\0';
    S->error_frame = fp;
    S->control = CONTROL_NONE;

    Value result = NIL_VAL;
    if (setjmp(fp->buf) == 0)
    {
        result = art_run_ast(S, program);
        S->error_frame = fp->prev;
        S->control = CONTROL_NONE;
    }
    else
    {
        // Error path. Reset every field a longjmp could have left
        // partially mutated. active_class is easy to forget — if
        // it isn't cleared here, the next REPL line runs with the
        // previous method's class still active, so privacy checks
        // and `super` see the wrong owner.
        S->error_frame = fp->prev;
        S->scope = S->global_scope;
        S->active_class = NULL;
        S->control = CONTROL_NONE;
        S->return_value = NIL_VAL;
        S->frame_count = 0;
        S->last_error = true;
        result = NIL_VAL;
    }

    node_free_tree(&program);

    S->current_file = saved_file;
    return result;
}

ObjString *value_to_string(ArtState *S, Value v)
{
    switch (v.type)
    {
    case VAL_NIL:
        return obj_string_from_utf8(S, "nil", 3);

    case VAL_BOOL:
        return AS_BOOL(v)
                   ? obj_string_from_utf8(S, "true", 4)
                   : obj_string_from_utf8(S, "false", 5);

    case VAL_INT:
        return obj_string_from_fmt(S, "%lld", (long long)AS_INT(v));

    case VAL_FLOAT:
        return obj_string_from_fmt(S, "%g", AS_FLOAT(v));

    case VAL_OBJ:
        break;
    }

    if (IS_STRING(v))
        return AS_STRING(v);

    FEATURES_FOR_EACH(f)
    {
        if (f->to_string == NULL)
            continue;
        ObjString *out;
        if (f->to_string(S, v, &out))
            return out;
    }

    return obj_string_from_fmt(S, "<%s>", value_type_name(v));
}
