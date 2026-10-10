#ifndef ART_VALUE_H
#define ART_VALUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct ArtState ArtState;
typedef struct ObjClass ObjClass;
typedef struct ObjInterface ObjInterface;
typedef struct ObjEnum ObjEnum;
typedef struct ObjScope ObjScope;
typedef struct Node Node;

typedef enum
{
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ,
} ValueType;

typedef struct Obj Obj;

typedef struct Value
{
    ValueType type;
    union
    {
        bool boolean;
        int64_t integer;
        double floating;
        Obj *obj;
    } as;
} Value;

#define NIL_VAL ((Value){VAL_NIL, {.integer = 0}})
#define BOOL_VAL(b) ((Value){VAL_BOOL, {.boolean = (b)}})
#define INT_VAL(i) ((Value){VAL_INT, {.integer = (i)}})
#define FLOAT_VAL(f) ((Value){VAL_FLOAT, {.floating = (f)}})
#define OBJ_VAL(o) ((Value){VAL_OBJ, {.obj = (Obj *)(o)}})

#define IS_NIL(v) ((v).type == VAL_NIL)
#define IS_BOOL(v) ((v).type == VAL_BOOL)
#define IS_INT(v) ((v).type == VAL_INT)
#define IS_FLOAT(v) ((v).type == VAL_FLOAT)
#define IS_NUMBER(v) (IS_INT(v) || IS_FLOAT(v))

#define AS_BOOL(v) ((v).as.boolean)
#define AS_INT(v) ((v).as.integer)
#define AS_FLOAT(v) ((v).as.floating)
#define AS_NUMBER(v) (IS_INT(v) ? (double)AS_INT(v) : AS_FLOAT(v))

#define OBJ_TYPES(X)                          \
    X(STRING, ObjString, "string")            \
    X(TABLE, ObjTable, "table")               \
    X(FUNCTION, ObjFunction, "function")      \
    X(CLOSURE, ObjClosure, "function")        \
    X(CLASS, ObjClass, "class")               \
    X(INSTANCE, ObjInstance, "instance")      \
    X(BOUND_METHOD, ObjBoundMethod, "method") \
    X(NATIVE, ObjNative, "native")            \
    X(ENUM, ObjEnum, "enum")                  \
    X(ENUM_VALUE, ObjEnumValue, "enum value") \
    X(SCOPE, ObjScope, "scope")               \
    X(INTERFACE, ObjInterface, "interface")

typedef enum
{
#define X(name, type, str) OBJ_##name,
    OBJ_TYPES(X)
#undef X
    OBJ_TYPE_COUNT
} ObjType;

struct Obj
{
    ObjType type;
    bool marked;
    size_t size;
    Obj *next;
};

typedef struct ObjString
{
    Obj header;
    uint32_t hash;
    int unit_count;
    int char_count;
    uint16_t *chars;
} ObjString;

typedef struct TableEntry
{
    ObjString *key;
    Value value;
} TableEntry;

typedef struct ObjTable
{
    Obj header;
    int array_count;
    int array_capacity;
    Value *array;
    int hash_count;
    int hash_capacity;
    TableEntry *entries;
    bool frozen;
} ObjTable;

typedef struct Param
{
    ObjString *name;
    ObjString *type_name;
    struct Node *default_value;
    struct ObjClass *type_class;
    ObjInterface *type_interface;
} Param;

typedef struct ObjFunction
{
    Obj header;
    ObjString *name;
    int arity;
    Param *params;
    Node *body;
    ObjClass *owner_class;
    bool is_static;
    bool is_constructor;
    bool is_getter;
    bool is_setter;
    bool is_operator;
    int operator_op;
    bool is_private;
    bool is_variadic;
} ObjFunction;

typedef struct ObjClosure
{
    Obj header;
    ObjFunction *function;
    ObjScope *captured_scope;
    ObjClass *owner_class;
} ObjClosure;

typedef struct Field
{
    ObjString *name;
    Value default_value;
    bool is_private;
    bool is_static;
    bool is_const;
} Field;

typedef struct ObjClass
{
    Obj header;
    ObjString *name;
    ObjClass *superclass;
    bool is_interface;

    Field *fields;
    int field_count;

    ObjTable *static_methods;

    ObjTable *methods;
    ObjTable *getters;
    ObjTable *setters;
    ObjTable *statics;

    ObjInterface **interfaces;
    int interface_count;
} ObjClass;

typedef struct ObjInstance
{
    Obj header;
    ObjClass *klass;
    ObjTable *fields;
    bool is_frozen;
    void *userdata;
    void (*userdata_free)(void *);
} ObjInstance;

typedef struct ObjBoundMethod
{
    Obj header;
    Value receiver;
    Value method;
    ObjClass *start_class;
} ObjBoundMethod;

typedef Value (*NativeFn)(ArtState *S, int argc, Value *argv);

typedef struct ObjNative
{
    Obj header;
    NativeFn fn;
    ObjString *name;
    int arity;
} ObjNative;

typedef struct ObjEnum
{
    Obj header;
    ObjString *name;
    ObjTable *members;
    ObjTable *methods;
    struct ObjEnumValue **ordered;
    int member_count;
} ObjEnum;

typedef struct ObjEnumValue
{
    Obj header;
    ObjEnum *parent;
    ObjString *name;
    Value value;
} ObjEnumValue;

typedef struct ObjScope
{
    Obj header;
    ObjTable *vars;
    ObjTable *consts;
    struct ObjScope *parent;
} ObjScope;

// An interface declares a set of member requirements. Each method
// requirement carries its own accepted arity range:
//
//   fun f(a, b)         arity = 2, variadic = false  -> accepts exactly 2
//   fun f(a, ...rest)   arity = 1, variadic = true   -> accepts any >= 1
//   fun f(...rest)      arity = 0, variadic = true   -> accepts any >= 0
//
// The `arity` value for a variadic requirement is the number of
// fixed parameters before the `...`, not the total parameter count.
typedef struct ObjInterface
{
    Obj header;
    ObjString *name;

    ObjInterface **parents;
    int parent_count;

    ObjString **method_names;
    int *method_arities;
    bool *method_variadics;
    int method_count;

    ObjString **getter_names;
    int getter_count;

    ObjString **setter_names;
    int setter_count;
} ObjInterface;

#define IS_OBJ(v) ((v).type == VAL_OBJ)
#define AS_OBJ(v) ((v).as.obj)
#define OBJ_TYPE(v) (AS_OBJ(v)->type)

static inline bool IS_OBJ_TYPE(Value v, ObjType t)
{
    return IS_OBJ(v) && OBJ_TYPE(v) == t;
}

#define X(name, type, str)                 \
    static inline bool IS_##name(Value v)  \
    {                                      \
        return IS_OBJ_TYPE(v, OBJ_##name); \
    }                                      \
    static inline type *AS_##name(Value v) \
    {                                      \
        return (type *)AS_OBJ(v);          \
    }
OBJ_TYPES(X)
#undef X

#define IS_CALLABLE(v) \
    (IS_CLOSURE(v) || IS_NATIVE(v) || IS_BOUND_METHOD(v))

bool value_equal(Value a, Value b);
uint32_t value_hash(Value v);
bool value_is_falsy(Value v);
const char *value_type_name(Value v);

bool value_matches_type(Value v, ObjString *type_name,
                        ObjClass *type_class,
                        ObjInterface *type_interface);

bool interface_satisfies(ObjInterface *a, ObjInterface *b);

ObjString *obj_string_from_utf8(ArtState *S, const char *utf8, int byte_len);
ObjString *obj_string_from_fmt(ArtState *S, const char *fmt, ...);
ObjString *obj_string_new_utf16(ArtState *S, const uint16_t *units, int unit_count);
ObjString *obj_string_take_utf16(ArtState *S, uint16_t *units, int unit_count);
ObjString *obj_string_intern_utf16(ArtState *S, const uint16_t *units, int unit_count);

ObjString *obj_string_concat(ArtState *S, ObjString *a, ObjString *b);
ObjString *obj_string_concat_all(ArtState *S, ObjString **pieces, int count);
ObjString *obj_string_substring(ArtState *S, ObjString *s,
                                int start_char, int end_char);
char *obj_string_to_utf8(ObjString *s);
int obj_string_length(ObjString *s);

void obj_table_free(ArtState *S, ObjTable *t);

ObjTable *obj_table_new(ArtState *S);
Value table_get(ObjTable *t, ObjString *key);
bool table_has(ObjTable *t, ObjString *key);
bool table_set(ArtState *S, ObjTable *t, ObjString *key, Value v);
bool table_delete(ObjTable *t, ObjString *key);
Value table_get_index(ObjTable *t, int index);
void table_push(ArtState *S, ObjTable *t, Value v);
int table_length(ObjTable *t);

ObjFunction *obj_function_new(ArtState *S, ObjString *name);
ObjClosure *obj_closure_new(ArtState *S, ObjFunction *fn);
ObjClass *obj_class_new(ArtState *S, ObjString *name, ObjClass *superclass);
ObjInstance *obj_instance_new(ArtState *S, ObjClass *klass);
ObjBoundMethod *obj_bound_method_new(ArtState *S, Value receiver, Value method);
ObjNative *obj_native_new(ArtState *S, NativeFn fn, ObjString *name, int arity);
ObjEnum *obj_enum_new(ArtState *S, ObjString *name);
ObjEnumValue *obj_enum_value_new(ArtState *S, ObjEnum *parent,
                                 ObjString *name, Value value);
ObjScope *obj_scope_new(ArtState *S, ObjScope *parent);
ObjInterface *obj_interface_new(ArtState *S, ObjString *name);

bool obj_string_eq_ascii(ObjString *s, const char *lit);

bool value_type_name_is_builtin(ObjString *name);

#endif // ART_VALUE_H
