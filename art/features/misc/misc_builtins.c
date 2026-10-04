// ============================================================
// misc_builtins.c — print, tostring, error, attempt
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
    if (!IS_STRING(argv[0]))
        art_runtime_error(S, S->current_node, "error expects a string");

    char *msg = obj_string_to_utf8(AS_STRING(argv[0]));
    art_runtime_error(S, S->current_node, "%s", msg);
    return NIL_VAL;
}

static Value attempt_build_result(ArtState *S, bool ok,
                                  Value value, const char *msg)
{
    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    table_push(S, out, BOOL_VAL(ok));
    if (ok)
    {
        GC_PUSH(S, value);
        table_push(S, out, value);
        GC_POP(S, 1);
    }
    else
    {
        ObjString *m = obj_string_from_utf8(S, msg, (int)strlen(msg));
        GC_PUSH(S, OBJ_VAL(m));
        table_push(S, out, OBJ_VAL(m));
        GC_POP(S, 1);
    }

    GC_POP(S, 1);
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

    ErrorFrame frame;
    frame.prev = S->error_frame;
    frame.file_name = S->current_file;
    frame.suppress_output = true;
    frame.message[0] = '\0';
    S->error_frame = &frame;

    Value result = NIL_VAL;
    bool ok = true;

    if (setjmp(frame.buf) == 0)
    {
        result = call_any(S, callable, sub_argc, sub_argv, NULL);
    }
    else
    {
        ok = false;
    }

    S->error_frame = saved_frame;
    S->scope = saved_scope;
    S->control = saved_control;
    S->return_value = saved_return;
    S->frame_count = saved_frames;
    S->active_class = saved_class;

    return attempt_build_result(S, ok, result, frame.message);
}

static void misc_register_builtins(ArtState *S)
{
    art_define_native(S, S->global_scope->vars, "print",    builtin_print,    -1);
    art_define_native(S, S->global_scope->vars, "tostring", builtin_tostring,  1);
    art_define_native(S, S->global_scope->vars, "error",    builtin_error,     1);
    art_define_native(S, S->global_scope->vars, "attempt",  builtin_attempt,  -1);
}

Feature misc_feature = {
    .name = "misc",
    .register_builtins = misc_register_builtins,
};
