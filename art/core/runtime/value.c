// ============================================================
// value.c — value-layer operations
//
// Equality, hashing, truthiness, type-name lookup, and the
// type-matching predicate used by overload resolution and `is`.
//
// Object-type dispatch routes through Feature hooks first, then
// falls back to core defaults (pointer comparison, pointer hash).
// Today only the string feature claims a hook (value_hash).
// ============================================================

#include <string.h>
#include <stdio.h>

#include "value.h"
#include "features/features.h"

bool value_equal(Value a, Value b)
{
    if (IS_NUMBER(a) && IS_NUMBER(b))
    {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }

    if (a.type != b.type)
        return false;

    if (a.type == VAL_OBJ)
    {
        FEATURES_FOR_EACH(f)
        {
            if (f->value_equal == NULL)
                continue;
            bool out;
            if (f->value_equal(a, b, &out))
                return out;
        }
    }

    switch (a.type)
    {
    case VAL_NIL:
        return true;
    case VAL_BOOL:
        return a.as.boolean == b.as.boolean;
    case VAL_INT:
        return a.as.integer == b.as.integer;
    case VAL_FLOAT:
        return a.as.floating == b.as.floating;
    case VAL_OBJ:
        return a.as.obj == b.as.obj;
    }
    return false;
}

static uint32_t hash_bits(uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return (uint32_t)x;
}

uint32_t value_hash(Value v)
{
    switch (v.type)
    {
    case VAL_NIL:
        return 0;

    case VAL_BOOL:
        return v.as.boolean ? 1u : 2u;

    case VAL_INT:
        return hash_bits((uint64_t)v.as.integer);

    case VAL_FLOAT:
    {
        double d = v.as.floating;
        if (d == (double)(int64_t)d)
        {
            return hash_bits((uint64_t)(int64_t)d);
        }
        uint64_t bits;
        memcpy(&bits, &d, sizeof(bits));
        return hash_bits(bits);
    }

    case VAL_OBJ:
    {
        FEATURES_FOR_EACH(f)
        {
            if (f->value_hash == NULL)
                continue;
            uint32_t out;
            if (f->value_hash(v, &out))
                return out;
        }
        return hash_bits((uint64_t)(uintptr_t)v.as.obj);
    }
    }
    return 0;
}

bool value_is_falsy(Value v)
{
    if (v.type == VAL_NIL)
        return true;
    if (v.type == VAL_BOOL)
        return !v.as.boolean;
    return false;
}

const char *value_type_name(Value v)
{
    switch (v.type)
    {
    case VAL_NIL:
        return "nil";
    case VAL_BOOL:
        return "bool";
    case VAL_INT:
        return "int";
    case VAL_FLOAT:
        return "float";

    case VAL_OBJ:
        switch (AS_OBJ(v)->type)
        {
#define X(name, type, str) case OBJ_##name: return str;
            OBJ_TYPES(X)
#undef X
        case OBJ_TYPE_COUNT:
            break;
        }
        return "object";
    }
    return "unknown";
}

bool obj_string_eq_ascii(ObjString *s, const char *lit)
{
    size_t len = strlen(lit);
    if ((size_t)s->unit_count != len)
        return false;
    for (size_t i = 0; i < len; i++)
    {
        if (s->chars[i] != (uint16_t)(unsigned char)lit[i])
            return false;
    }
    return true;
}

static bool class_is_a(ObjClass *klass, ObjClass *target)
{
    for (ObjClass *c = klass; c != NULL; c = c->superclass)
    {
        if (c == target)
            return true;
    }
    return false;
}

static bool interface_is_a(ObjInterface *iface, ObjInterface *target)
{
    if (iface == target)
        return true;
    for (int i = 0; i < iface->parent_count; i++)
        if (interface_is_a(iface->parents[i], target))
            return true;
    return false;
}

bool value_matches_type(Value v, ObjString *type_name,
                        ObjClass *type_class,
                        ObjInterface *type_interface)
{
    if (type_name == NULL)
        return true;

    if (type_class != NULL)
        return IS_INSTANCE(v) &&
               class_is_a(AS_INSTANCE(v)->klass, type_class);

    if (type_interface != NULL)
    {
        if (!IS_INSTANCE(v))
            return false;
        ObjClass *k = AS_INSTANCE(v)->klass;
        for (int i = 0; i < k->interface_count; i++)
        {
            if (interface_is_a(k->interfaces[i], type_interface))
                return true;
        }
        return false;
    }

    if (obj_string_eq_ascii(type_name, "Any"))
        return true;
    if (obj_string_eq_ascii(type_name, "Number"))
        return IS_NUMBER(v);
    if (obj_string_eq_ascii(type_name, "Int"))
        return IS_INT(v);
    if (obj_string_eq_ascii(type_name, "Float"))
        return IS_FLOAT(v);
    if (obj_string_eq_ascii(type_name, "String"))
        return IS_STRING(v);
    if (obj_string_eq_ascii(type_name, "Bool"))
        return IS_BOOL(v);
    if (obj_string_eq_ascii(type_name, "Nil"))
        return IS_NIL(v);
    if (obj_string_eq_ascii(type_name, "Table"))
        return IS_TABLE(v);
    if (obj_string_eq_ascii(type_name, "Function"))
        return IS_CALLABLE(v);
    return false;
}

bool interface_satisfies(ObjInterface *a, ObjInterface *b)
{
    return interface_is_a(a, b);
}

bool value_type_name_is_builtin(ObjString *name)
{
    return obj_string_eq_ascii(name, "Any") ||
           obj_string_eq_ascii(name, "Number") ||
           obj_string_eq_ascii(name, "Int") ||
           obj_string_eq_ascii(name, "Float") ||
           obj_string_eq_ascii(name, "String") ||
           obj_string_eq_ascii(name, "Bool") ||
           obj_string_eq_ascii(name, "Nil") ||
           obj_string_eq_ascii(name, "Table") ||
           obj_string_eq_ascii(name, "Function");
}
