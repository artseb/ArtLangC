#ifndef ART_INTERFACE_H
#define ART_INTERFACE_H

#include "ast.h"
#include "value.h"
#include "state.h"

typedef struct Parser Parser;

typedef struct InterfaceDeclNode
{
    Node base;
    ObjString *name;
    ObjString **parents;
    int parent_count;
    ObjString **method_names;
    int *method_arities;
    bool *method_variadics;
    int method_count;
    ObjString **getter_names;
    int getter_count;
    ObjString **setter_names;
    int setter_count;
} InterfaceDeclNode;

Node *interface_parse(Parser *P);

Node *node_interface_decl(int line, int col,
                          ObjString *name,
                          ObjString **parents, int parent_count,
                          ObjString **method_names, int *method_arities,
                          bool *method_variadics,
                          int method_count,
                          ObjString **getter_names, int getter_count,
                          ObjString **setter_names, int setter_count);

bool interface_free_children(Node *n);
const char *interface_node_name(NodeType t);

bool interface_gc_blacken(ArtState *S, Obj *o);
bool interface_gc_free_buffers(ArtState *S, Obj *o);

Value eval_interface_decl(ArtState *S, Node *n);

bool interface_satisfies(ObjInterface *a, ObjInterface *b);

int interface_missing_members(ObjClass *klass, ObjInterface *iface,
                              ObjString **missing, int missing_cap);

typedef struct Feature Feature;
extern Feature interface_feature;

#endif