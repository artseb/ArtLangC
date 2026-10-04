#include "interface.h"
#include "gc.h"

ObjInterface *obj_interface_new(ArtState *S, ObjString *name)
{
    ObjInterface *iface = ALLOCATE_OBJ(S, ObjInterface, OBJ_INTERFACE);
    iface->name = name;
    iface->parents = NULL;
    iface->parent_count = 0;
    iface->method_names = NULL;
    iface->method_arities = NULL;
    iface->method_count = 0;
    iface->getter_names = NULL;
    iface->getter_count = 0;
    iface->setter_names = NULL;
    iface->setter_count = 0;
    return iface;
}
