// ============================================================
// test_value.c — standalone tests for the value layer
//
// No language, no parser, no interpreter. Just ArtState +
// strings + tables + GC. If this passes clean under ASan and
// valgrind, the memory foundation is solid enough to build
// the lexer on top of.
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "value.h"
#include "gc.h"

static int g_run = 0;
static int g_failed = 0;

#define CHECK(cond, msg)                          \
    do                                            \
    {                                             \
        g_run++;                                  \
        if (!(cond))                              \
        {                                         \
            g_failed++;                           \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", \
                    __FILE__, __LINE__, (msg));   \
        }                                         \
    } while (0)

// ============================================================
// Strings
// ============================================================

static void test_string_interning(void)
{
    ArtState *S = art_state_new();

    ObjString *a = obj_string_from_utf8(S, "hello", 5);
    ObjString *b = obj_string_from_utf8(S, "hello", 5);
    ObjString *c = obj_string_from_utf8(S, "world", 5);

    CHECK(a != NULL, "interned string is not NULL");
    CHECK(a == b, "same content -> same pointer (interned)");
    CHECK(a != c, "different content -> different pointer");
    CHECK(a->hash == b->hash, "same content -> same hash");
    CHECK(a->unit_count == 5, "ascii unit count");
    CHECK(a->hash != c->hash, "different content -> different hash");

    art_state_free(S);
}

static void test_string_length(void)
{
    ArtState *S = art_state_new();

    // Pure ASCII
    ObjString *a = obj_string_from_utf8(S, "hello", 5);
    CHECK(obj_string_length(a) == 5, "ascii: length 5");

    // é is 2 UTF-8 bytes, 1 UTF-16 unit
    ObjString *b = obj_string_from_utf8(S, "caf\xc3\xa9", 5);
    CHECK(b->unit_count == 4, "cafe: 4 UTF-16 units");
    CHECK(obj_string_length(b) == 4, "cafe: 4 codepoints");

    // 🙂 U+1F642: 4 UTF-8 bytes, 2 UTF-16 units (surrogate pair), 1 codepoint
    ObjString *e = obj_string_from_utf8(S, "\xf0\x9f\x99\x82", 4);
    CHECK(e->unit_count == 2, "emoji: 2 UTF-16 units (surrogate pair)");
    CHECK(obj_string_length(e) == 1, "emoji: 1 codepoint");

    art_state_free(S);
}

static void test_string_concat(void)
{
    ArtState *S = art_state_new();

    ObjString *a = obj_string_from_utf8(S, "hello", 5);
    ObjString *b = obj_string_from_utf8(S, " world", 6);
    ObjString *c = obj_string_concat(S, a, b);

    CHECK(c->unit_count == 11, "concat: 11 units");

    char *u8 = obj_string_to_utf8(c);
    CHECK(u8 != NULL && strcmp(u8, "hello world") == 0, "concat content");
    free(u8);

    art_state_free(S);
}

static void test_string_substring(void)
{
    ArtState *S = art_state_new();

    ObjString *s = obj_string_from_utf8(S, "hello world", 11);

    ObjString *a = obj_string_substring(S, s, 1, 5);
    char *ua = obj_string_to_utf8(a);
    CHECK(strcmp(ua, "hello") == 0, "substring 1..5");
    free(ua);

    ObjString *b = obj_string_substring(S, s, 7, 11);
    char *ub = obj_string_to_utf8(b);
    CHECK(strcmp(ub, "world") == 0, "substring 7..11");
    free(ub);

    // Empty when end < start
    ObjString *c = obj_string_substring(S, s, 5, 3);
    CHECK(c->unit_count == 0, "substring with end<start is empty");

    art_state_free(S);
}

// ============================================================
// Tables — hash part
// ============================================================

static void test_table_hash(void)
{
    ArtState *S = art_state_new();
    ObjTable *t = obj_table_new(S);

    ObjString *k1 = obj_string_from_utf8(S, "hp", 2);
    ObjString *k2 = obj_string_from_utf8(S, "mp", 2);

    CHECK(table_set(S, t, k1, INT_VAL(100)), "set hp is new");
    CHECK(table_set(S, t, k2, INT_VAL(50)), "set mp is new");

    CHECK(AS_INT(table_get(t, k1)) == 100, "get hp");
    CHECK(AS_INT(table_get(t, k2)) == 50, "get mp");

    ObjString *missing = obj_string_from_utf8(S, "xyz", 3);
    CHECK(IS_NIL(table_get(t, missing)), "missing key returns nil");

    CHECK(!table_set(S, t, k1, INT_VAL(200)), "overwrite is not new");
    CHECK(AS_INT(table_get(t, k1)) == 200, "get hp after overwrite");

    art_state_free(S);
}

// ============================================================
// Tables — array part
// ============================================================

static void test_table_array(void)
{
    ArtState *S = art_state_new();
    ObjTable *t = obj_table_new(S);

    table_push(S, t, INT_VAL(10));
    table_push(S, t, INT_VAL(20));
    table_push(S, t, INT_VAL(30));

    CHECK(table_length(t) == 3, "array length 3");
    CHECK(AS_INT(table_get_index(t, 1)) == 10, "index 1");
    CHECK(AS_INT(table_get_index(t, 2)) == 20, "index 2");
    CHECK(AS_INT(table_get_index(t, 3)) == 30, "index 3");
    CHECK(IS_NIL(table_get_index(t, 0)), "index 0 out of bounds");
    CHECK(IS_NIL(table_get_index(t, 4)), "index 4 out of bounds");

    // Grow past initial capacity (8) to exercise the resize path
    for (int i = 0; i < 20; i++)
        table_push(S, t, INT_VAL(i));
    CHECK(table_length(t) == 23, "array grew to 23");
    CHECK(AS_INT(table_get_index(t, 23)) == 19, "last pushed value");

    art_state_free(S);
}

// ============================================================
// Tables — delete
// ============================================================

static void test_table_delete(void)
{
    ArtState *S = art_state_new();
    ObjTable *t = obj_table_new(S);

    ObjString *k = obj_string_from_utf8(S, "key", 3);
    table_set(S, t, k, INT_VAL(42));
    CHECK(AS_INT(table_get(t, k)) == 42, "before delete");

    CHECK(table_delete(t, k), "delete existing");
    CHECK(IS_NIL(table_get(t, k)), "after delete returns nil");
    CHECK(!table_delete(t, k), "delete missing returns false");

    // Insert after delete should work (tombstone reuse)
    CHECK(table_set(S, t, k, INT_VAL(7)), "reinsert is new");
    CHECK(AS_INT(table_get(t, k)) == 7, "reinserted value");

    art_state_free(S);
}

// ============================================================
// GC
// ============================================================

static void test_gc_sweeps_cycle(void)
{
    ArtState *S = art_state_new();

    // Build a cycle: t["self"] = t. Nothing references t from
    // the roots (stack is empty, no globals). The GC should be
    // able to free it — which refcounting alone could not.
    ObjTable *t = obj_table_new(S);
    ObjString *self = obj_string_from_utf8(S, "self", 4);
    table_set(S, t, self, OBJ_VAL(t));

    size_t before = S->gc.bytes_allocated;
    art_gc_collect(S);
    size_t after = S->gc.bytes_allocated;

    CHECK(after < before, "cycle was swept (bytes decreased)");

    art_state_free(S);
}

static void test_gc_preserves_reachable(void)
{
    ArtState *S = art_state_new();

    ObjTable *t = obj_table_new(S);
    ObjString *k = obj_string_from_utf8(S, "key", 3);
    table_set(S, t, k, INT_VAL(999));

    // Root both across the collection
    GC_PUSH(S, OBJ_VAL(t));
    GC_PUSH(S, OBJ_VAL(k));

    art_gc_collect(S);

    CHECK(AS_INT(table_get(t, k)) == 999, "reachable value survives GC");

    GC_POP(S, 2);
    art_state_free(S);
}

static void test_gc_strings_survive(void)
{
    ArtState *S = art_state_new();

    ObjString *a = obj_string_from_utf8(S, "persistent", 10);
    art_gc_collect(S);

    // Interning puts every string in S->strings, which is a root.
    // So even without a GC_PUSH, this string must survive.
    char *u8 = obj_string_to_utf8(a);
    CHECK(strcmp(u8, "persistent") == 0, "interned string survives GC");
    free(u8);

    art_state_free(S);
}

// ============================================================
// Equality / hashing on primitives
// ============================================================

static void test_value_equality(void)
{
    CHECK(value_equal(INT_VAL(5), INT_VAL(5)), "int == int");
    CHECK(value_equal(FLOAT_VAL(5.0), FLOAT_VAL(5.0)), "float == float");
    CHECK(value_equal(INT_VAL(5), FLOAT_VAL(5.0)), "int == integral float");
    CHECK(!value_equal(INT_VAL(5), INT_VAL(6)), "int != int (different)");
    CHECK(value_equal(NIL_VAL, NIL_VAL), "nil == nil");
    CHECK(value_equal(BOOL_VAL(true), BOOL_VAL(true)), "true == true");
    CHECK(!value_equal(BOOL_VAL(true), BOOL_VAL(false)), "true != false");
}

static void test_value_truthiness(void)
{
    CHECK(value_is_falsy(NIL_VAL), "nil is falsy");
    CHECK(value_is_falsy(BOOL_VAL(false)), "false is falsy");
    CHECK(!value_is_falsy(BOOL_VAL(true)), "true is truthy");
    CHECK(!value_is_falsy(INT_VAL(0)), "0 is truthy (locked decision)");
    CHECK(!value_is_falsy(INT_VAL(1)), "1 is truthy");
}

// ============================================================
// Entry point
// ============================================================

int main(void)
{
    printf("ART value layer tests\n");
    printf("=====================\n");

    test_string_interning();
    test_string_length();
    test_string_concat();
    test_string_substring();

    test_table_hash();
    test_table_array();
    test_table_delete();

    test_gc_sweeps_cycle();
    test_gc_preserves_reachable();
    test_gc_strings_survive();

    test_value_equality();
    test_value_truthiness();

    printf("\n%d checks, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}