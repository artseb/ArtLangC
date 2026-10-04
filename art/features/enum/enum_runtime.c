#include "enum.h"
#include "gc.h"

ObjEnum *obj_enum_new(ArtState *S, ObjString *name)
{
    ObjEnum *e = ALLOCATE_OBJ(S, ObjEnum, OBJ_ENUM);
    GC_PUSH(S, OBJ_VAL(e));

    e->name = name;
    e->members = obj_table_new(S);
    e->methods = obj_table_new(S);
    e->ordered = NULL;
    e->member_count = 0;

    GC_POP(S, 1);
    return e;
}

ObjEnumValue *obj_enum_value_new(ArtState *S, ObjEnum *parent,
                                 ObjString *name, Value value)
{
    ObjEnumValue *ev = ALLOCATE_OBJ(S, ObjEnumValue, OBJ_ENUM_VALUE);
    ev->parent = parent;
    ev->name = name;
    ev->value = value;
    return ev;
}
