// ============================================================
// error.c — runtime error reporting and the thrown-value path
//
// Two ways to fail:
//
//   art_runtime_error — formats a message, stores the formatted
//     string as the thrown value, prints, longjmps.
//
//   art_throw_value   — takes a raw Value (any type), stores it
//     as-is, prints a string form for display, longjmps. Used by
//     the error() builtin so error(t) can throw a table, an
//     instance, or nil, and attempt() hands it back unchanged.
//
// Both set S->thrown_value before longjmp, so the catch site
// (either attempt, or the top-level art_run_source) can read the
// value the error carries.
// ============================================================

#include "interp.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Print the current call stack. Shared by both error paths.
// Frame 0 is the outermost; we walk from the top down.
static void print_stack_trace(ArtState *S)
{
    if (S == NULL)
        return;

    int printed = 0;
    for (int i = S->frame_count - 1; i >= 0 && printed < 20; i--)
    {
        ObjClosure *cl = S->frames[i].closure;
        if (cl == NULL || cl->function == NULL)
            continue;

        ObjFunction *fn = cl->function;
        char *fname_u8 = fn->name ? obj_string_to_utf8(fn->name) : NULL;
        const char *fname = fname_u8 ? fname_u8 : "<lambda>";

        Node *cs = S->frames[i].call_site;
        const char *cf = S->frames[i].file_name
                             ? S->frames[i].file_name
                             : "?";

        if (cs)
            fprintf(stderr, "  at %s (%s:%d:%d)\n",
                    fname, cf, cs->line, cs->column);
        else
            fprintf(stderr, "  at %s\n", fname);

        free(fname_u8);
        printed++;
    }

    if (S->frame_count > 20)
        fprintf(stderr, "  ... (%d more)\n", S->frame_count - 20);
}

// Raise a runtime error with a formatted message. Stores the
// resulting string as the thrown value so attempt() can return
// it, then longjmps (or prints-and-exits if no frame is set).
void art_runtime_error(ArtState *S, Node *at, const char *fmt, ...)
{
    ErrorFrame *frame = S ? S->error_frame : NULL;

    const char *file = (frame && frame->file_name)
                           ? frame->file_name
                           : "<runtime>";
    int line = at ? at->line : 0;
    int col = at ? at->column : 0;

    char buf[512];
    int n = snprintf(buf, sizeof(buf), "%s:%d:%d: error: ", file, line, col);
    if (n < 0)
        n = 0;

    va_list ap;
    va_start(ap, fmt);
    if ((size_t)n < sizeof(buf))
        vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    va_end(ap);

    // Store the message as the thrown value. Interning can trigger
    // a GC; the resulting string is reachable via S->strings, and
    // we set S->thrown_value right after, which mark_roots keeps
    // alive across the longjmp.
    if (S != NULL)
    {
        ObjString *msg = obj_string_from_utf8(S, buf, (int)strlen(buf));
        S->thrown_value = OBJ_VAL(msg);
    }

    if (frame == NULL || !frame->suppress_output)
    {
        fprintf(stderr, "%s\n", buf);
        print_stack_trace(S);
    }

    if (frame)
        longjmp(frame->buf, 1);

    exit(1);
}

// Throw an arbitrary value. The raw value is stored; a string
// form is generated only when the error will be printed.
void art_throw_value(ArtState *S, Node *at, Value v)
{
    ErrorFrame *frame = S ? S->error_frame : NULL;
    bool will_print = (frame == NULL || !frame->suppress_output);

    if (will_print)
    {
        const char *file = (frame && frame->file_name)
                               ? frame->file_name
                               : "<runtime>";
        int line = at ? at->line : 0;
        int col = at ? at->column : 0;

        ObjString *vs = value_to_string(S, v);
        char *vu8 = obj_string_to_utf8(vs);

        fprintf(stderr, "%s:%d:%d: error: %s\n", file, line, col, vu8);
        free(vu8);

        print_stack_trace(S);
    }

    if (S != NULL)
        S->thrown_value = v;

    if (frame)
        longjmp(frame->buf, 1);

    exit(1);
}