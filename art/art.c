// ============================================================
// art.c — the embed API
// ============================================================

#include "art.h"
#include "state.h"
#include "interp.h"
#include "interrupt.h"
#include "register.h"
#include "features/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ArtState *art_open(void)
{
    ArtState *S = art_state_new();
    art_register_builtins(S);
    return S;
}

void art_close(ArtState *S)
{
    art_state_free(S);
}

bool art_run_string(ArtState *S, const char *source, const char *name)
{
    art_run_source(S, source, (int)strlen(source), name);
    return !S->last_error;
}

bool art_run_file(ArtState *S, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "art: cannot open '%s'\n", path);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        fprintf(stderr, "art: cannot seek '%s'\n", path);
        return false;
    }
    long size = ftell(f);
    if (size < 0)
    {
        fclose(f);
        fprintf(stderr, "art: cannot read '%s'\n", path);
        return false;
    }
    fseek(f, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    if (!buf)
    {
        fclose(f);
        fprintf(stderr, "art: out of memory reading '%s'\n", path);
        return false;
    }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';

    bool ok = art_run_string(S, buf, path);
    free(buf);
    return ok;
}

void art_register_native(ArtState *S, const char *name,
                         NativeFn fn, int arity)
{
    art_define_native(S, S->global_scope->vars, name, fn, arity);
}
