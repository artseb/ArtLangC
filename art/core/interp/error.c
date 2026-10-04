#include "interp.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Print "<file>:<line>:<col>: error: <message>" and unwind to the
// nearest ErrorFrame. Never returns.
void art_runtime_error(ArtState *S, Node *at, const char *fmt, ...)
{
    ErrorFrame *frame = S ? S->error_frame : NULL;

    const char *file = (frame && frame->file_name) ? frame->file_name : "<runtime>";
    int line = at ? at->line : 0;
    int col = at ? at->column : 0;

    va_list ap;

    if (frame && frame->suppress_output)
    {
        int n = snprintf(frame->message, sizeof(frame->message),
                         "%s:%d:%d: ", file, line, col);
        va_start(ap, fmt);
        vsnprintf(frame->message + n, sizeof(frame->message) - n, fmt, ap);
        va_end(ap);
    }
    else
    {
        fprintf(stderr, "%s:%d:%d: error: ", file, line, col);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);

        // Stack trace. Top frame first.
        if (S != NULL)
        {
            int printed = 0;
            for (int i = S->frame_count - 1; i >= 0 && printed < 20; i--)
            {
                ObjClosure *cl = S->frames[i].closure;
                if (cl == NULL || cl->function == NULL)
                    continue;

                ObjFunction *fn = cl->function;
                const char *fname = fn->name
                                        ? obj_string_to_utf8(fn->name)
                                        : "<lambda>";

                Node *cs = S->frames[i].call_site;
                const char *cf = S->frames[i].file_name
                                     ? S->frames[i].file_name
                                     : "?";

                if (cs)
                    fprintf(stderr, "  at %s (%s:%d:%d)\n",
                            fname, cf, cs->line, cs->column);
                else
                    fprintf(stderr, "  at %s\n", fname);

                printed++;
            }

            if (S->frame_count > 20)
                fprintf(stderr, "  ... (%d more)\n", S->frame_count - 20);
        }
    }

    if (frame)
        longjmp(frame->buf, 1);

    exit(1);
}
