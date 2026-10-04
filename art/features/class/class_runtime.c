#include "class.h"
#include "gc.h"

ObjClass *obj_class_new(ArtState *S, ObjString *name, ObjClass *superclass)
{
    ObjClass *c = ALLOCATE_OBJ(S, ObjClass, OBJ_CLASS);
    GC_PUSH(S, OBJ_VAL(c));

    c->name = name;
    c->superclass = superclass;
    c->is_interface = false;
    c->fields = NULL;
    c->field_count = 0;

    c->static_methods = obj_table_new(S);

    c->methods = obj_table_new(S);
    c->getters = obj_table_new(S);
    c->setters = obj_table_new(S);
    c->statics = obj_table_new(S);

    c->interfaces = NULL;
    c->interface_count = 0;

    GC_POP(S, 1);
    return c;
}

ObjInstance *obj_instance_new(ArtState *S, ObjClass *klass)
{
    ObjInstance *inst = ALLOCATE_OBJ(S, ObjInstance, OBJ_INSTANCE);
    GC_PUSH(S, OBJ_VAL(inst));

    inst->klass = klass;
    inst->fields = obj_table_new(S);
    inst->is_frozen = false;
    inst->userdata = NULL;
    inst->userdata_free = NULL;

    GC_POP(S, 1);
    return inst;
}

ObjBoundMethod *obj_bound_method_new(ArtState *S, Value receiver, Value method)
{
    ObjBoundMethod *bm = ALLOCATE_OBJ(S, ObjBoundMethod, OBJ_BOUND_METHOD);
    bm->receiver = receiver;
    bm->method = method;
    bm->start_class = NULL;
    return bm;
}
