#include "enum.h"
#include "gc.h"

#include <stdlib.h>

bool enum_gc_blacken(ArtState *S, Obj *o)
{
    switch (o->type)
    {
    case OBJ_ENUM:
    {
        ObjEnum *e = (ObjEnum *)o;
        art_gc_mark_object(S, (Obj *)e->name);
        art_gc_mark_object(S, (Obj *)e->members);
        art_gc_mark_object(S, (Obj *)e->methods);
        for (int i = 0; i < e->member_count; i++)
            art_gc_mark_object(S, (Obj *)e->ordered[i]);
        return true;
    }
    case OBJ_ENUM_VALUE:
    {
        ObjEnumValue *ev = (ObjEnumValue *)o;
        art_gc_mark_object(S, (Obj *)ev->parent);
        art_gc_mark_object(S, (Obj *)ev->name);
        art_gc_mark_value(S, ev->value);
        return true;
    }
    default:
        return false;
    }
}

bool enum_gc_free_buffers(ArtState *S, Obj *o)
{
    if (o->type != OBJ_ENUM) return false;
    (void)S;
    ObjEnum *e = (ObjEnum *)o;
    free(e->ordered);
    return true;
}
