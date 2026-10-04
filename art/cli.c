// ============================================================
// art — command-line interface
//
//   art file.art   run a script
//   art            REPL (one statement per line)
//   art -h         usage
//
// The REPL shares one ArtState across the whole session, so
// globals persist. Runtime errors print and the session keeps
// going — state is reset inside art_run_source.
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "state.h"
#include "interp.h"

static const char *USAGE =
    "usage: art [file.art]\n"
    "       art            start the REPL\n"
    "       art -h         this message\n";

// Prints a value the way tostring() would. Duplicated from
// builtin_core.c on purpose — the REPL output is a UI concern
// and should be able to diverge from the language's tostring.
static void print_value(Value v)
{
    switch (v.type)
    {
    case VAL_NIL:
        fputs("nil", stdout);
        return;
    case VAL_BOOL:
        fputs(AS_BOOL(v) ? "true" : "false", stdout);
        return;
    case VAL_INT:
        printf("%lld", (long long)AS_INT(v));
        return;
    case VAL_FLOAT:
        printf("%g", AS_FLOAT(v));
        return;
    case VAL_OBJ:
        if (IS_STRING(v))
        {
            char *u = obj_string_to_utf8(AS_STRING(v));
            fputs(u, stdout);
            free(u);
            return;
        }
        printf("<%s>", value_type_name(v));
        return;
    }
}

// ============================================================
// Balance check for REPL input.
//
// Returns true if braces are balanced (submit the buffer),
// false if we're inside an open block (keep reading).
//
// Skips content inside strings and comments so `"{"` doesn't
// break the counter.
// ============================================================

static bool input_is_balanced(const char *src, int len)
{
    int depth = 0;
    bool in_string = false;
    bool in_line_comment = false;
    bool in_block_comment = false;
    int block_depth = 0;

    for (int i = 0; i < len; i++)
    {
        char c = src[i];

        if (in_line_comment)
        {
            if (c == '\n')
                in_line_comment = false;
            continue;
        }
        if (in_block_comment)
        {
            if (c == '*' && i + 1 < len && src[i + 1] == '/')
            {
                block_depth--;
                if (block_depth == 0)
                    in_block_comment = false;
                i++;
            }
            else if (c == '/' && i + 1 < len && src[i + 1] == '*')
            {
                block_depth++;
                i++;
            }
            continue;
        }
        if (in_string)
        {
            if (c == '\\' && i + 1 < len)
            {
                i++;
                continue;
            }
            if (c == '"')
                in_string = false;
            continue;
        }

        if (c == '"')
        {
            in_string = true;
            continue;
        }
        if (c == '/' && i + 1 < len && src[i + 1] == '/')
        {
            in_line_comment = true;
            i++;
            continue;
        }
        if (c == '/' && i + 1 < len && src[i + 1] == '*')
        {
            in_block_comment = true;
            block_depth = 1;
            i++;
            continue;
        }
        if (c == '{')
            depth++;
        else if (c == '}')
            depth--;
    }

    return depth <= 0;
}

static int repl(ArtState *S)
{
    static char buf[1 << 16]; // 64 KB — plenty for REPL input
    int buf_len = 0;

    printf("ART 0.1  (Ctrl-D to exit, blank line to cancel)\n");

    for (;;)
    {
        fputs(buf_len == 0 ? "> " : "... ", stdout);
        fflush(stdout);

        char line[4096];
        if (!fgets(line, sizeof(line), stdin))
        {
            fputc('\n', stdout);
            return 0;
        }

        int len = (int)strlen(line);

        // Empty line while buffering -> cancel. Empty line at
        // top level -> ignore.
        if (len == 0 || (len == 1 && line[0] == '\n'))
        {
            if (buf_len > 0)
            {
                printf("(cancelled)\n");
                buf_len = 0;
            }
            continue;
        }

        // quit/exit only at top level.
        if (buf_len == 0)
        {
            // Trim trailing whitespace for comparison.
            int t = len;
            while (t > 0 && (line[t - 1] == '\n' || line[t - 1] == '\r' ||
                             line[t - 1] == ' ' || line[t - 1] == '\t'))
                t--;
            if (t == 4 && strncmp(line, "quit", 4) == 0)
                return 0;
            if (t == 4 && strncmp(line, "exit", 4) == 0)
                return 0;
        }

        // Append.
        if (buf_len + len >= (int)sizeof(buf) - 1)
        {
            fprintf(stderr, "input too long, discarded\n");
            buf_len = 0;
            continue;
        }
        memcpy(buf + buf_len, line, len);
        buf_len += len;
        buf[buf_len] = '\0';

        // Submit only when braces balance.
        if (!input_is_balanced(buf, buf_len))
            continue;

        Value v = art_run_source(S, buf, buf_len, "<repl>");

        if (!S->last_error && !IS_NIL(v))
        {
            print_value(v);
            fputc('\n', stdout);
        }

        buf_len = 0;
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        fputs(USAGE, stdout);
        return 0;
    }

    ArtState *S = art_open();

    int rc;
    if (argc == 1)
    {
        rc = repl(S);
    }
    else if (argc == 2)
    {
        rc = art_run_file(S, argv[1]) ? 0 : 1;
    }
    else
    {
        fputs(USAGE, stderr);
        art_close(S);
        return 2;
    }

    art_close(S);
    return rc;
}
