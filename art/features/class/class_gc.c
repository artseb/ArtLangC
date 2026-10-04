// ============================================================
// class_gc.c — GC hooks for ObjClass, ObjInstance, ObjBoundMethod
// ============================================================

#include "class.h"
#include "gc.h"
#include <stdlib.h>

bool class_gc_blacken(ArtState *S, Obj *o)
{
    switch (o->type)
    {
    case OBJ_CLASS:
    {
        ObjClass *c = (ObjClass *)o;
        art_gc_mark_object(S, (Obj *)c->name);
        art_gc_mark_object(S, (Obj *)c->superclass);

        for (int i = 0; i < c->field_count; i++)
        {
            art_gc_mark_object(S, (Obj *)c->fields[i].name);
            art_gc_mark_value(S, c->fields[i].default_value);
        }

        art_gc_mark_object(S, (Obj *)c->static_methods);
        art_gc_mark_object(S, (Obj *)c->methods);
        art_gc_mark_object(S, (Obj *)c->getters);
        art_gc_mark_object(S, (Obj *)c->setters);
        art_gc_mark_object(S, (Obj *)c->statics);

        for (int i = 0; i < c->interface_count; i++)
        {
            art_gc_mark_object(S, (Obj *)c->interfaces[i]);
        }
        return true;
    }

    case OBJ_INSTANCE:
    {
        ObjInstance *inst = (ObjInstance *)o;
        art_gc_mark_object(S, (Obj *)inst->klass);
        art_gc_mark_object(S, (Obj *)inst->fields);
        return true;
    }

    case OBJ_BOUND_METHOD:
    {
        ObjBoundMethod *bm = (ObjBoundMethod *)o;
        art_gc_mark_value(S, bm->receiver);
        art_gc_mark_value(S, bm->method);
        art_gc_mark_object(S, (Obj *)bm->start_class);
        return true;
    }

    default:
        return false;
    }
}

bool class_gc_free_buffers(ArtState *S, Obj *o)
{
    (void)S;
    if (o->type != OBJ_CLASS) return false;

    ObjClass *c = (ObjClass *)o;
    if (c->fields) { free(c->fields); c->fields = NULL; }
    if (c->interfaces) { free(c->interfaces); c->interfaces = NULL; }
    return true;
}
