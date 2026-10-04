// ============================================================
// file_class.c — text file read/write
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "feature.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "register.h"

static Value get_this(ArtState *S)
{
    ObjString *tn = obj_string_from_utf8(S, "this", 4);
    Value v;
    if (!art_scope_lookup(S->scope, tn, NULL, &v))
        v = NIL_VAL;
    return v;
}

static FILE *instance_file(ArtState *S, Value self, Node *at)
{
    if (!IS_INSTANCE(self))
        art_runtime_error(S, at, "'this' is not a File");
    ObjInstance *inst = AS_INSTANCE(self);
    if (inst->userdata == NULL)
        art_runtime_error(S, at, "file is closed");
    return (FILE *)inst->userdata;
}

static void file_userdata_free(void *p)
{
    FILE *fp = (FILE *)p;
    if (fp)
        fclose(fp);
}

static char *path_from(Value v)
{
    if (!IS_STRING(v))
        return NULL;
    return obj_string_to_utf8(AS_STRING(v));
}

static Value file_read(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    char *path = path_from(argv[0]);
    if (path == NULL)
        return NIL_VAL;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        free(path);
        return NIL_VAL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0)
    {
        fclose(fp);
        free(path);
        return NIL_VAL;
    }

    char *buf = malloc((size_t)size + 1);
    if (buf == NULL)
    {
        fclose(fp);
        free(path);
        return NIL_VAL;
    }

    size_t got = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    free(path);
    buf[got] = '\0';

    ObjString *s = obj_string_from_utf8(S, buf, (int)got);
    free(buf);
    return OBJ_VAL(s);
}

static void write_file_or_error(ArtState *S, Value path_v, Value data_v,
                                const char *mode, Node *at)
{
    char *path = path_from(path_v);
    if (path == NULL)
        art_runtime_error(S, at, "File: path must be a string");
    if (!IS_STRING(data_v))
        art_runtime_error(S, at, "File: contents must be a string");

    FILE *fp = fopen(path, mode);
    if (fp == NULL)
        art_runtime_error(S, at, "File: cannot open '%s'", path);

    char *data = obj_string_to_utf8(AS_STRING(data_v));
    size_t len = strlen(data);
    if (fwrite(data, 1, len, fp) != len)
    {
        fclose(fp);
        free(data);
        art_runtime_error(S, at, "File: write failed on '%s'", path);
    }
    fclose(fp);
    free(data);
    free(path);
}

static Value file_write(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    write_file_or_error(S, argv[0], argv[1], "wb", NULL);
    return NIL_VAL;
}

static Value file_append_static(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    write_file_or_error(S, argv[0], argv[1], "ab", NULL);
    return NIL_VAL;
}

static Value file_delete(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    char *path = path_from(argv[0]);
    if (path == NULL)
        art_runtime_error(S, NULL, "File: path must be a string");
    if (remove(path) != 0)
        art_runtime_error(S, NULL, "File: cannot delete '%s'", path);
    free(path);
    return NIL_VAL;
}

static Value file_open(ArtState *S, int argc, Value *argv)
{
    if (S->file_class == NULL)
        art_runtime_error(S, NULL, "File class not registered");

    char *path = path_from(argv[0]);
    if (path == NULL)
        art_runtime_error(S, NULL, "File.open: path must be a string");

    const char *mode = "rb";
    char *mode_alloc = NULL;
    if (argc >= 2 && IS_STRING(argv[1]))
    {
        mode_alloc = obj_string_to_utf8(AS_STRING(argv[1]));
        mode = mode_alloc;
    }

    FILE *fp = fopen(path, mode);
    if (fp == NULL)
    {
        art_runtime_error(S, NULL, "File.open: cannot open '%s'", path);
    }
    free(path);
    free(mode_alloc);

    ObjInstance *inst = obj_instance_new(S, S->file_class);
    inst->userdata = fp;
    inst->userdata_free = file_userdata_free;
    return OBJ_VAL(inst);
}

static Value file_inst_read(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    (void)argv;
    Value self = get_this(S);
    FILE *fp = instance_file(S, self, NULL);

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    for (;;)
    {
        if (len + 4096 > cap)
        {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        size_t n = fread(buf + len, 1, 4096, fp);
        if (n == 0)
            break;
        len += n;
    }
    buf[len] = '\0';

    ObjString *s = obj_string_from_utf8(S, buf, (int)len);
    free(buf);
    return OBJ_VAL(s);
}

static Value file_inst_read_line(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    (void)argv;
    Value self = get_this(S);
    FILE *fp = instance_file(S, self, NULL);

    size_t cap = 128, len = 0;
    char *buf = malloc(cap);
    int c;

    while ((c = fgetc(fp)) != EOF)
    {
        if (c == '\n')
            break;
        if (len + 1 >= cap)
        {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        buf[len++] = (char)c;
    }

    if (c == EOF && len == 0)
    {
        free(buf);
        return NIL_VAL;
    }

    if (len > 0 && buf[len - 1] == '\r')
        len--;
    buf[len] = '\0';

    ObjString *s = obj_string_from_utf8(S, buf, (int)len);
    free(buf);
    return OBJ_VAL(s);
}

static Value file_inst_read_lines(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    (void)argv;

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    for (;;)
    {
        Value line = file_inst_read_line(S, 0, NULL);
        if (IS_NIL(line))
            break;
        GC_PUSH(S, line);
        table_push(S, out, line);
        GC_POP(S, 1);
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value file_inst_write(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    FILE *fp = instance_file(S, self, NULL);

    if (!IS_STRING(argv[0]))
        art_runtime_error(S, NULL, "f.write: argument must be a string");

    char *data = obj_string_to_utf8(AS_STRING(argv[0]));
    size_t len = strlen(data);
    if (fwrite(data, 1, len, fp) != len)
    {
        free(data);
        art_runtime_error(S, NULL, "f.write: write failed");
    }
    free(data);
    return NIL_VAL;
}

static Value file_inst_append(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    FILE *fp = instance_file(S, self, NULL);

    if (!IS_STRING(argv[0]))
        art_runtime_error(S, NULL, "f.append: argument must be a string");

    if (fseek(fp, 0, SEEK_END) != 0)
        art_runtime_error(S, NULL, "f.append: seek failed");

    char *data = obj_string_to_utf8(AS_STRING(argv[0]));
    size_t len = strlen(data);
    if (fwrite(data, 1, len, fp) != len)
    {
        free(data);
        art_runtime_error(S, NULL, "f.append: write failed");
    }
    free(data);
    return NIL_VAL;
}

static Value file_inst_close(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    (void)argv;
    Value self = get_this(S);
    if (!IS_INSTANCE(self))
        return NIL_VAL;
    ObjInstance *inst = AS_INSTANCE(self);
    if (inst->userdata && inst->userdata_free)
        inst->userdata_free(inst->userdata);
    inst->userdata = NULL;
    inst->userdata_free = NULL;
    return NIL_VAL;
}

static void file_register_builtins(ArtState *S)
{
    ObjString *name = obj_string_from_utf8(S, "File", 4);
    GC_PUSH(S, OBJ_VAL(name));
    ObjClass *klass = obj_class_new(S, name, NULL);
    GC_PUSH(S, OBJ_VAL(klass));

    S->file_class = klass;

    art_define_method(S, klass, "read",      file_inst_read,      0);
    art_define_method(S, klass, "readLine",  file_inst_read_line, 0);
    art_define_method(S, klass, "readLines", file_inst_read_lines, 0);
    art_define_method(S, klass, "write",     file_inst_write,     1);
    art_define_method(S, klass, "append",    file_inst_append,    1);
    art_define_method(S, klass, "close",     file_inst_close,     0);

    art_define_native(S, klass->statics, "read",   file_read,          1);
    art_define_native(S, klass->statics, "write",  file_write,         2);
    art_define_native(S, klass->statics, "append", file_append_static, 2);
    art_define_native(S, klass->statics, "delete", file_delete,        1);
    art_define_native(S, klass->statics, "open",   file_open,         -1);

    table_set(S, S->global_scope->vars, name, OBJ_VAL(klass));

    GC_POP(S, 2);
}

Feature file_feature = {
    .name = "file",
    .register_builtins = file_register_builtins,
};
