// POSIX.1-2008 + XSI (_XOPEN_SOURCE 700) for strdup, realpath, nanosleep. Under -std=c11 glibc hides
// them, and an implicit strdup() returns int, truncating the pointer on
// 64-bit. Must come before the first system include.
#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif

// ============================================================
// import_builtin.c — the `import(path)` builtin
// ============================================================

#include "import.h"
#include "feature.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "parser.h"
#include "register.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

static char *canonicalize(const char *path)
{
#ifdef _WIN32
    return _fullpath(NULL, path, 0);
#else
    char buf[4096];
    if (realpath(path, buf) == NULL)
        return NULL;
    return strdup(buf);
#endif
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

static char *resolve_import_path(const char *importer_file, const char *spec)
{
    if (spec[0] == '/' || spec[0] == '\\')
        return NULL;
#ifdef _WIN32
    if (spec[0] != '\0' && spec[1] == ':')
        return NULL;
#endif

    char dir[2048];
    if (importer_file != NULL)
    {
        strncpy(dir, importer_file, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
    }
    else
    {
        strcpy(dir, ".");
    }

    char *slash = strrchr(dir, '/');
#ifdef _WIN32
    char *bslash = strrchr(dir, '\\');
    if (bslash && (!slash || bslash > slash))
        slash = bslash;
#endif
    if (slash)
        *slash = '\0';
    else
        strcpy(dir, ".");

    char combined[4096];
    snprintf(combined, sizeof(combined), "%s/%s", dir, spec);

    char *canon = NULL;

    if (file_exists(combined))
    {
        canon = canonicalize(combined);
        if (canon != NULL)
            return canon;
    }

    char with_ext[sizeof(combined) + 8];
    snprintf(with_ext, sizeof(with_ext), "%s.art", combined);
    if (file_exists(with_ext))
    {
        canon = canonicalize(with_ext);
        if (canon != NULL)
            return canon;
    }

    return NULL;
}

static char *read_file(const char *path, long *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0)
    {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    *out_size = (long)got;
    return buf;
}

static Value builtin_import(ArtState *S, int argc, Value *argv)
{
    if (argc != 1 || !IS_STRING(argv[0]))
        art_runtime_error(S, NULL, "import expects one string argument");

    char *spec = obj_string_to_utf8(AS_STRING(argv[0]));
    char *abs_path = resolve_import_path(S->current_file, spec);
    free(spec);

    if (abs_path == NULL)
        art_runtime_error(S, NULL, "cannot resolve import");

    ObjString *key = obj_string_from_utf8(S, abs_path, (int)strlen(abs_path));

    Value cached = table_get(S->import_cache, key);
    if (!IS_NIL(cached))
    {
        free(abs_path);
        return cached;
    }

    if (table_has(S->imports_in_progress, key))
    {
        free(abs_path);
        art_runtime_error(S, NULL, "cyclic import");
    }

    long size = 0;
    char *source = read_file(abs_path, &size);
    if (source == NULL)
    {
        char *p = abs_path;
        abs_path = NULL;
        art_runtime_error(S, NULL, "cannot read '%s'", p);
    }

    Node *program = parse_source(S, source, (int)size, abs_path);
    if (program == NULL)
    {
        free(source);
        free(abs_path);
        if (S->error_frame != NULL)
            longjmp(S->error_frame->buf, 1);
        return NIL_VAL;
    }

    table_set(S, S->imports_in_progress, key, BOOL_VAL(true));

    ObjScope *saved_scope = S->scope;
    const char *saved_file = S->current_file;

    S->scope = obj_scope_new(S, S->global_scope);
    S->current_file = abs_path;

    ErrorFrame frame;
    ErrorFrame *volatile fp = &frame;
    fp->prev = S->error_frame;
    fp->file_name = abs_path;
    fp->suppress_output = false;
    fp->message[0] = '\0';
    S->error_frame = fp;

    S->control = CONTROL_NONE;
    Value volatile result = NIL_VAL;

    if (setjmp(fp->buf) == 0)
    {
        art_run_ast(S, program);

        if (S->control == CONTROL_RETURN)
            result = S->return_value;
        S->control = CONTROL_NONE;

        S->error_frame = fp->prev;
        S->scope = saved_scope;
        S->current_file = saved_file;

        table_delete(S->imports_in_progress, key);
        node_free_tree(&program);
        free(source);
        free(abs_path);

        table_set(S, S->import_cache, key, result);
        return result;
    }
    else
    {
        S->error_frame = fp->prev;
        S->scope = saved_scope;
        S->current_file = saved_file;

        table_delete(S->imports_in_progress, key);
        node_free_tree(&program);
        free(source);
        free(abs_path);

        if (S->error_frame != NULL)
            longjmp(S->error_frame->buf, 1);
        return NIL_VAL;
    }
}

static void import_register_builtins(ArtState *S)
{
    art_define_native(S, S->global_scope->vars, "import",
                      builtin_import, 1);
}

Feature import_feature = {
    .name = "import",
    .register_builtins = import_register_builtins,
};
