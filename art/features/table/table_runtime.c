// ============================================================
// table_runtime.c — ObjTable construction, array ops, hash ops
// ============================================================

#include <string.h>
#include "value.h"
#include "state.h"

#define TABLE_MIN_CAPACITY 8
#define TABLE_MAX_LOAD 0.75

ObjTable *obj_table_new(ArtState *S)
{
    ObjTable *t = ALLOCATE_OBJ(S, ObjTable, OBJ_TABLE);
    t->array_count = 0;
    t->array_capacity = 0;
    t->array = NULL;
    t->hash_count = 0;
    t->hash_capacity = 0;
    t->entries = NULL;
    t->frozen = false;
    return t;
}

void obj_table_free(ArtState *S, ObjTable *t)
{
    art_realloc(S, t->array, sizeof(Value) * t->array_capacity, 0);
    art_realloc(S, t->entries, sizeof(TableEntry) * t->hash_capacity, 0);
}

static TableEntry *find_entry(TableEntry *entries, int capacity, ObjString *key)
{
    uint32_t idx = key->hash & (capacity - 1);
    TableEntry *tombstone = NULL;

    for (;;)
    {
        TableEntry *entry = &entries[idx];

        if (entry->key == NULL)
        {
            if (IS_NIL(entry->value))
            {
                return tombstone ? tombstone : entry;
            }
            else
            {
                if (!tombstone)
                    tombstone = entry;
            }
        }
        else if (entry->key == key)
        {
            return entry;
        }

        idx = (idx + 1) & (capacity - 1);
    }
}

static void adjust_hash_capacity(ArtState *S, ObjTable *t, int capacity)
{
    TableEntry *entries = art_realloc(S, NULL, 0, sizeof(TableEntry) * capacity);
    for (int i = 0; i < capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    t->hash_count = 0;
    for (int i = 0; i < t->hash_capacity; i++)
    {
        TableEntry *old = &t->entries[i];
        if (old->key == NULL)
            continue;

        TableEntry *dest = find_entry(entries, capacity, old->key);
        dest->key = old->key;
        dest->value = old->value;
        t->hash_count++;
    }

    art_realloc(S, t->entries, sizeof(TableEntry) * t->hash_capacity, 0);
    t->entries = entries;
    t->hash_capacity = capacity;
}

Value table_get(ObjTable *t, ObjString *key)
{
    if (t->hash_count == 0)
        return NIL_VAL;
    TableEntry *entry = find_entry(t->entries, t->hash_capacity, key);
    if (entry->key == NULL)
        return NIL_VAL;
    return entry->value;
}

bool table_has(ObjTable *t, ObjString *key)
{
    if (t->hash_count == 0)
        return false;
    TableEntry *entry = find_entry(t->entries, t->hash_capacity, key);
    return entry->key != NULL;
}

bool table_set(ArtState *S, ObjTable *t, ObjString *key, Value v)
{
    if (t->frozen)
        return false;

    if (t->hash_count + 1 > t->hash_capacity * TABLE_MAX_LOAD)
    {
        int cap = t->hash_capacity < TABLE_MIN_CAPACITY
                      ? TABLE_MIN_CAPACITY
                      : t->hash_capacity * 2;
        adjust_hash_capacity(S, t, cap);
    }

    TableEntry *entry = find_entry(t->entries, t->hash_capacity, key);
    bool is_new = (entry->key == NULL);
    if (is_new && IS_NIL(entry->value))
        t->hash_count++;

    entry->key = key;
    entry->value = v;
    return is_new;
}

// Deletes the key if present. Returns true if something was removed.
// Uses a tombstone so the probe chain stays intact.
bool table_delete(ObjTable *t, ObjString *key)
{
    if (t->hash_count == 0)
        return false;

    TableEntry *entry = find_entry(t->entries, t->hash_capacity, key);
    if (entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_push(ArtState *S, ObjTable *t, Value v)
{
    if (t->array_count + 1 > t->array_capacity)
    {
        int old_cap = t->array_capacity;
        int new_cap = old_cap < 8 ? 8 : old_cap * 2;
        t->array = art_realloc(S, t->array,
                               sizeof(Value) * old_cap,
                               sizeof(Value) * new_cap);
        t->array_capacity = new_cap;
    }
    t->array[t->array_count++] = v;
}

Value table_get_index(ObjTable *t, int index)
{
    if (index < 1 || index > t->array_count)
        return NIL_VAL;
    return t->array[index - 1];
}

int table_length(ObjTable *t)
{
    return t->array_count;
}
