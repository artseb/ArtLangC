// ============================================================
// table_methods.c — the Table class methods
// ============================================================

#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "state.h"
#include "value.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "register.h"

static Value get_this(ArtState *S)
{
    Value v;
    if (!art_scope_lookup(S->scope, S->this_name, NULL, &v))
        v = NIL_VAL;
    return v;
}

// t.push(v1, v2, ...) -> t, appending all arguments.
// Variadic; any number of args including zero.
static Value table_push_method(ArtState *S, int argc, Value *argv)
{
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);
    for (int i = 0; i < argc; i++)
        table_push(S, t, argv[i]);
    return self;
}

static Value table_length_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    return INT_VAL(AS_TABLE(self)->array_count);
}

static Value table_pop_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);
    if (t->array_count == 0) return NIL_VAL;
    return t->array[t->array_count - 1];
}

static Value table_is_empty_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return BOOL_VAL(true);
    ObjTable *t = AS_TABLE(self);
    return BOOL_VAL(t->array_count == 0 && t->hash_count == 0);
}

// t.contains(v) -> bool. Searches the array part first, then
// the values in the hash part. Both halves are checked because
// a table can have both, and "contains" reads as "does this
// value appear anywhere in the table". Keys are not checked
// — use `k in t` for that.
static Value table_contains_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return BOOL_VAL(false);
    ObjTable *t = AS_TABLE(self);

    for (int i = 0; i < t->array_count; i++)
        if (value_equal(t->array[i], argv[0]))
            return BOOL_VAL(true);

    for (int i = 0; i < t->hash_capacity; i++)
    {
        TableEntry *e = &t->entries[i];
        if (e->key == NULL) continue;
        if (value_equal(e->value, argv[0]))
            return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}

static Value table_index_of_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);
    for (int i = 0; i < t->array_count; i++)
        if (value_equal(t->array[i], argv[0]))
            return INT_VAL(i + 1);
    return NIL_VAL;
}

static Value table_clear_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return self;
    ObjTable *t = AS_TABLE(self);

    t->array_count = 0;
    for (int i = 0; i < t->hash_capacity; i++)
        t->entries[i].key = NULL;
    t->hash_count = 0;
    return self;
}

static Value table_reverse_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return self;
    ObjTable *t = AS_TABLE(self);
    int i = 0, j = t->array_count - 1;
    while (i < j)
    {
        Value tmp = t->array[i];
        t->array[i] = t->array[j];
        t->array[j] = tmp;
        i++; j--;
    }
    return self;
}

static Value table_clone_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    for (int i = 0; i < t->array_count; i++)
        table_push(S, out, t->array[i]);

    for (int i = 0; i < t->hash_capacity; i++)
    {
        TableEntry *e = &t->entries[i];
        if (e->key != NULL)
            table_set(S, out, e->key, e->value);
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value table_keys_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    for (int i = 0; i < t->array_count; i++)
        table_push(S, out, INT_VAL(i + 1));

    for (int i = 0; i < t->hash_capacity; i++)
    {
        TableEntry *e = &t->entries[i];
        if (e->key != NULL)
            table_push(S, out, OBJ_VAL(e->key));
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value table_values_method(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    for (int i = 0; i < t->array_count; i++)
        table_push(S, out, t->array[i]);

    for (int i = 0; i < t->hash_capacity; i++)
    {
        TableEntry *e = &t->entries[i];
        if (e->key != NULL)
            table_push(S, out, e->value);
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value table_insert_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return self;
    ObjTable *t = AS_TABLE(self);

    int i = (int)AS_INT(argv[0]);
    if (i < 1) i = 1;
    if (i > t->array_count + 1) i = t->array_count + 1;

    table_push(S, t, NIL_VAL);
    for (int j = t->array_count - 1; j >= i; j--)
        t->array[j] = t->array[j - 1];
    t->array[i - 1] = argv[1];
    return self;
}

static Value table_remove_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);

    int i = (int)AS_INT(argv[0]);
    if (i < 1 || i > t->array_count) return NIL_VAL;

    Value removed = t->array[i - 1];
    for (int j = i - 1; j < t->array_count - 1; j++)
        t->array[j] = t->array[j + 1];
    t->array_count--;
    return removed;
}

static Value table_slice_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;
    ObjTable *t = AS_TABLE(self);

    int a = (int)AS_INT(argv[0]);
    int b = (int)AS_INT(argv[1]);
    if (a < 1) a = 1;
    if (b > t->array_count) b = t->array_count;

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    for (int i = a; i <= b; i++)
        table_push(S, out, t->array[i - 1]);

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value table_join_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self))
        return NIL_VAL;
    if (!IS_STRING(argv[0]))
        return NIL_VAL;

    ObjTable *t = AS_TABLE(self);
    ObjString *sep = AS_STRING(argv[0]);
    int n = t->array_count;

    if (n == 0)
        return OBJ_VAL(obj_string_from_utf8(S, "", 0));

    // Pass 1: stringify each element, sum total length. The
    // pieces are interned, so they're reachable through
    // S->strings and can't be collected by pass 2.
    ObjString **pieces = malloc(sizeof(ObjString *) * n);
    int total = 0;
    for (int i = 0; i < n; i++)
    {
        pieces[i] = value_to_string(S, t->array[i]);
        total += pieces[i]->unit_count;
        if (i > 0)
            total += sep->unit_count;
    }

    // Pass 2: single allocation, copy everything in.
    uint16_t *buf = malloc(sizeof(uint16_t) * total);
    int off = 0;
    for (int i = 0; i < n; i++)
    {
        if (i > 0)
        {
            memcpy(buf + off, sep->chars,
                   sizeof(uint16_t) * sep->unit_count);
            off += sep->unit_count;
        }
        memcpy(buf + off, pieces[i]->chars,
               sizeof(uint16_t) * pieces[i]->unit_count);
        off += pieces[i]->unit_count;
    }
    free(pieces);

    ObjString *result = obj_string_take_utf16(S, buf, total);
    return OBJ_VAL(result);
}

static Value call_callback(ArtState *S, Value fn, Value arg, Node *at)
{
    Value args[1] = {arg};
    return call_any(S, fn, 1, args, at);
}

static Value table_map_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;

    ObjTable *t = AS_TABLE(self);
    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    for (int i = 0; i < t->array_count; i++)
    {
        Value r = call_callback(S, argv[0], t->array[i], NULL);
        if (S->control != CONTROL_NONE) { GC_POP(S, 1); return NIL_VAL; }
        GC_PUSH(S, r);
        table_push(S, out, r);
        GC_POP(S, 1);
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value table_filter_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;

    ObjTable *t = AS_TABLE(self);
    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    for (int i = 0; i < t->array_count; i++)
    {
        Value v = t->array[i];
        Value pred = call_callback(S, argv[0], v, NULL);
        if (S->control != CONTROL_NONE) { GC_POP(S, 1); return NIL_VAL; }
        if (!value_is_falsy(pred))
        {
            GC_PUSH(S, v);
            table_push(S, out, v);
            GC_POP(S, 1);
        }
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

static Value table_reduce_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;

    ObjTable *t = AS_TABLE(self);
    Value acc = argv[1];

    for (int i = 0; i < t->array_count; i++)
    {
        Value args[2] = {acc, t->array[i]};
        acc = call_any(S, argv[0], 2, args, NULL);
        if (S->control != CONTROL_NONE) return NIL_VAL;
    }

    return acc;
}

static Value table_foreach_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;

    ObjTable *t = AS_TABLE(self);
    for (int i = 0; i < t->array_count; i++)
    {
        call_callback(S, argv[0], t->array[i], NULL);
        if (S->control != CONTROL_NONE) return NIL_VAL;
    }
    return NIL_VAL;
}

static Value table_any_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return BOOL_VAL(false);

    ObjTable *t = AS_TABLE(self);
    for (int i = 0; i < t->array_count; i++)
    {
        Value pred = call_callback(S, argv[0], t->array[i], NULL);
        if (S->control != CONTROL_NONE) return BOOL_VAL(false);
        if (!value_is_falsy(pred)) return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}

static Value table_all_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return BOOL_VAL(true);

    ObjTable *t = AS_TABLE(self);
    for (int i = 0; i < t->array_count; i++)
    {
        Value pred = call_callback(S, argv[0], t->array[i], NULL);
        if (S->control != CONTROL_NONE) return BOOL_VAL(false);
        if (value_is_falsy(pred)) return BOOL_VAL(false);
    }
    return BOOL_VAL(true);
}

static Value table_find_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;

    ObjTable *t = AS_TABLE(self);
    for (int i = 0; i < t->array_count; i++)
    {
        Value v = t->array[i];
        Value pred = call_callback(S, argv[0], v, NULL);
        if (S->control != CONTROL_NONE) return NIL_VAL;
        if (!value_is_falsy(pred)) return v;
    }
    return NIL_VAL;
}

static Value table_count_method(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_TABLE(self)) return INT_VAL(0);

    ObjTable *t = AS_TABLE(self);
    int count = 0;
    for (int i = 0; i < t->array_count; i++)
    {
        Value pred = call_callback(S, argv[0], t->array[i], NULL);
        if (S->control != CONTROL_NONE) return INT_VAL(0);
        if (!value_is_falsy(pred)) count++;
    }
    return INT_VAL(count);
}

static int string_compare(ObjString *a, ObjString *b)
{
    int m = a->unit_count < b->unit_count ? a->unit_count : b->unit_count;
    for (int i = 0; i < m; i++)
    {
        if (a->chars[i] != b->chars[i])
            return (a->chars[i] < b->chars[i]) ? -1 : 1;
    }
    return a->unit_count - b->unit_count;
}

static Value table_sort_method(ArtState *S, int argc, Value *argv)
{
    Value self = get_this(S);
    if (!IS_TABLE(self)) return NIL_VAL;

    ObjTable *t = AS_TABLE(self);
    int n = t->array_count;
    if (n <= 1) return self;

    Value cmp = (argc >= 1) ? argv[0] : NIL_VAL;

    for (int i = 1; i < n; i++)
    {
        Value key = t->array[i];
        int j = i - 1;

        while (j >= 0)
        {
            bool place_before;

            if (IS_NIL(cmp))
            {
                Value a = t->array[j];
                if (IS_NUMBER(a) && IS_NUMBER(key))
                {
                    place_before = AS_NUMBER(a) <= AS_NUMBER(key);
                }
                else if (IS_STRING(a) && IS_STRING(key))
                {
                    place_before = string_compare(AS_STRING(a),
                                                  AS_STRING(key)) <= 0;
                }
                else
                {
                    art_runtime_error(S, NULL,
                                      "sort: mixed or non-comparable element types");
                }
            }
            else
            {
                Value args[2] = {t->array[j], key};
                Value pred = call_any(S, cmp, 2, args, NULL);
                if (S->control != CONTROL_NONE) return NIL_VAL;
                place_before = !value_is_falsy(pred);
            }

            if (place_before) break;
            t->array[j + 1] = t->array[j];
            j--;
        }
        t->array[j + 1] = key;
    }

    return self;
}

void art_register_table_builtins(ArtState *S)
{
    ObjString *name = obj_string_from_utf8(S, "Table", 5);
    GC_PUSH(S, OBJ_VAL(name));
    ObjClass *klass = obj_class_new(S, name, NULL);
    GC_PUSH(S, OBJ_VAL(klass));

    S->builtin_classes[OBJ_TABLE] = klass;

    art_define_method(S, klass, "push",     table_push_method,    -1);
    art_define_method(S, klass, "pop",      table_pop_method,      0);
    art_define_method(S, klass, "length",   table_length_method,   0);
    art_define_method(S, klass, "isEmpty",  table_is_empty_method, 0);
    art_define_method(S, klass, "contains", table_contains_method, 1);
    art_define_method(S, klass, "indexOf",  table_index_of_method, 1);
    art_define_method(S, klass, "clear",    table_clear_method,    0);
    art_define_method(S, klass, "reverse",  table_reverse_method,  0);
    art_define_method(S, klass, "clone",    table_clone_method,    0);
    art_define_method(S, klass, "keys",     table_keys_method,     0);
    art_define_method(S, klass, "values",   table_values_method,   0);
    art_define_method(S, klass, "insert",   table_insert_method,   2);
    art_define_method(S, klass, "remove",   table_remove_method,   1);
    art_define_method(S, klass, "slice",    table_slice_method,    2);
    art_define_method(S, klass, "join",     table_join_method,     1);
    art_define_method(S, klass, "map",      table_map_method,      1);
    art_define_method(S, klass, "filter",   table_filter_method,   1);
    art_define_method(S, klass, "reduce",   table_reduce_method,   2);
    art_define_method(S, klass, "forEach",  table_foreach_method,  1);
    art_define_method(S, klass, "any",      table_any_method,      1);
    art_define_method(S, klass, "all",      table_all_method,      1);
    art_define_method(S, klass, "find",     table_find_method,     1);
    art_define_method(S, klass, "count",    table_count_method,    1);
    art_define_method(S, klass, "sort",     table_sort_method,    -1);

    GC_POP(S, 2);
}
