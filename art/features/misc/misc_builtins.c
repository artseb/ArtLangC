// ============================================================
// misc_builtins.c — print, tostring, error, attempt, assert
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "misc.h"
#include "feature.h"
#include "interp.h"
#include "gc.h"
#include "register.h"

static Value builtin_print(ArtState *S, int argc, Value *argv)
{
    for (int i = 0; i < argc; i++)
    {
        if (i > 0)
            fputc(' ', stdout);
        ObjString *s = value_to_string(S, argv[i]);
        char *utf8 = obj_string_to_utf8(s);
        fputs(utf8, stdout);
        free(utf8);
    }
    fputc('\n', stdout);
    return NIL_VAL;
}

static Value builtin_tostring(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    ObjString *s = value_to_string(S, argv[0]);
    return OBJ_VAL(s);
}

static Value builtin_error(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    art_throw_value(S, S->current_node, argv[0]);
    return NIL_VAL; // unreachable
}

// assert(cond) or assert(cond, message)
//
// Raises a runtime error when cond is falsy. The optional second
// argument is a message string included in the error output.
// Intended for invariants — things that should never happen.
// Unlike error(), which is for recoverable user-facing failures,
// assert() reads as "this is a bug if it fires".
static Value builtin_assert(ArtState *S, int argc, Value *argv)
{
    if (argc < 1)
        art_runtime_error(S, S->current_node,
                          "assert expects at least one argument");

    if (!value_is_falsy(argv[0]))
        return NIL_VAL;

    if (argc >= 2 && IS_STRING(argv[1]))
    {
        char *msg = obj_string_to_utf8(AS_STRING(argv[1]));
        art_runtime_error(S, S->current_node,
                          "assertion failed: %s", msg);
    }

    art_runtime_error(S, S->current_node, "assertion failed");
    return NIL_VAL;
}

static Value attempt_build_result(ArtState *S, bool ok,
                                  Value value, Value thrown)
{
    GC_PUSH(S, ok ? value : thrown);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    table_push(S, out, BOOL_VAL(ok));
    table_push(S, out, ok ? value : thrown);

    GC_POP(S, 2);
    return OBJ_VAL(out);
}

static Value builtin_attempt(ArtState *S, int argc, Value *argv)
{
    if (argc < 1)
        art_runtime_error(S, NULL, "attempt expects at least one argument");

    Value callable = argv[0];
    int sub_argc = argc - 1;
    Value *sub_argv = argv + 1;

    ErrorFrame *saved_frame = S->error_frame;
    ObjScope *saved_scope = S->scope;
    ControlFlow saved_control = S->control;
    Value saved_return = S->return_value;
    int saved_frames = S->frame_count;
    ObjClass *saved_class = S->active_class;
    Value saved_thrown = S->thrown_value;

    ErrorFrame frame;
    frame.prev = S->error_frame;
    frame.file_name = S->current_file;
    frame.suppress_output = true;
    frame.message[0] = '\0';
    S->error_frame = &frame;

    S->thrown_value = NIL_VAL;

    Value result = NIL_VAL;
    Value volatile thrown = NIL_VAL;
    bool ok = true;

    if (setjmp(frame.buf) == 0)
    {
        result = call_any(S, callable, sub_argc, sub_argv, NULL);
    }
    else
    {
        ok = false;
        thrown = S->thrown_value;
    }

    S->error_frame = saved_frame;
    S->scope = saved_scope;
    S->control = saved_control;
    S->return_value = saved_return;
    S->frame_count = saved_frames;
    S->active_class = saved_class;
    S->thrown_value = saved_thrown;

    return attempt_build_result(S, ok, result, thrown);
}

static void misc_register_builtins(ArtState *S)
{
    art_define_native(S, S->global_scope->vars, "print",    builtin_print,    -1);
    art_define_native(S, S->global_scope->vars, "tostring", builtin_tostring,  1);
    art_define_native(S, S->global_scope->vars, "error",    builtin_error,     1);
    art_define_native(S, S->global_scope->vars, "assert",   builtin_assert,   -1);
    art_define_native(S, S->global_scope->vars, "attempt",  builtin_attempt,  -1);
}

Feature misc_feature = {
    .name = "misc",
    .register_builtins = misc_register_builtins,
};
