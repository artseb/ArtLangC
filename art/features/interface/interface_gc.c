#include "interface.h"
#include "gc.h"
#include <stdlib.h>

bool interface_gc_blacken(ArtState *S, Obj *o)
{
    if (o->type != OBJ_INTERFACE)
        return false;

    ObjInterface *iface = (ObjInterface *)o;
    art_gc_mark_object(S, (Obj *)iface->name);

    for (int i = 0; i < iface->parent_count; i++)
        art_gc_mark_object(S, (Obj *)iface->parents[i]);
    for (int i = 0; i < iface->method_count; i++)
        art_gc_mark_object(S, (Obj *)iface->method_names[i]);
    for (int i = 0; i < iface->getter_count; i++)
        art_gc_mark_object(S, (Obj *)iface->getter_names[i]);
    for (int i = 0; i < iface->setter_count; i++)
        art_gc_mark_object(S, (Obj *)iface->setter_names[i]);

    return true;
}

bool interface_gc_free_buffers(ArtState *S, Obj *o)
{
    (void)S;
    if (o->type != OBJ_INTERFACE)
        return false;

    ObjInterface *iface = (ObjInterface *)o;
    free(iface->parents);
    free(iface->method_names);
    free(iface->method_arities);
    free(iface->method_variadics);
    free(iface->getter_names);
    free(iface->setter_names);
    return true;
}