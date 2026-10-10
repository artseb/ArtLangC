#ifndef ART_CLASS_H
#define ART_CLASS_H

#include "ast.h"
#include "value.h"
#include "state.h"

#include "features/interface/interface.h"

typedef struct Parser Parser;

Node *class_parse(Parser *P);
void class_parse_modifiers(Parser *P, bool *is_local, bool *is_static,
                           bool *is_const);

Node *node_class_decl(int line, int col, ObjString *name,
                      ObjString *super_name,
                      ObjString **implements_names, int implements_count,
                      Node **fields, int field_count,
                      Node **methods, int method_count);
Node *node_field_decl(int line, int col, ObjString *name,
                      Node *default_value,
                      bool is_private, bool is_static, bool is_const);
Node *node_method_decl(int line, int col, ObjFunction *fn);
Node *node_this(int line, int col);
Node *node_super(int line, int col);

ObjClass *obj_class_new(ArtState *S, ObjString *name, ObjClass *superclass);
ObjInstance *obj_instance_new(ArtState *S, ObjClass *klass);
ObjBoundMethod *obj_bound_method_new(ArtState *S, Value receiver, Value method);

Value eval_class_decl(ArtState *S, Node *n);
Value eval_this(ArtState *S, Node *n);
Value eval_super(ArtState *S, Node *n);
Value interp_this(ArtState *S, Node *at);

ObjClosure *class_find_method(ArtState *S, ObjClass *klass, ObjString *name,
                              int argc, Value *args, Node *at);
ObjClosure *class_find_method_any(ArtState *S, ObjClass *klass,
                                  ObjString *name);
ObjClosure *class_find_operator(ArtState *S, ObjClass *klass, TokenType op,
                                int argc, Value *args);

Value instance_get_member(ArtState *S, ObjInstance *inst,
                          ObjString *name, Node *at);
Value instance_set_member(ArtState *S, ObjInstance *inst,
                          ObjString *name, Value v, Node *at);
void class_register_reflection(ArtState *S, ObjClass *klass);
bool instance_try_get_member(ArtState *S, ObjInstance *inst,
                             ObjString *name, Value *out, Node *at);

bool class_free_children(Node *n);
const char *class_node_name(NodeType t);
bool class_gc_blacken(ArtState *S, Obj *o);
bool class_gc_free_buffers(ArtState *S, Obj *o);

Value class_instantiate(ArtState *S, ObjClass *klass,
                        int argc, Value *args, Node *at);

void class_resolve_param_types(ArtState *S, ObjFunction *fn);

typedef struct Feature Feature;
extern Feature class_feature;

typedef struct ClassDeclNode
{
    Node base;
    ObjString *name;
    ObjString *superclass_name;
    Node **fields;
    int field_count;
    Node **methods;
    int method_count;
    ObjString **implements_names;
    int implements_count;
} ClassDeclNode;

typedef struct FieldDeclNode
{
    Node base;
    ObjString *name;
    Node *default_value;
    bool is_private;
    bool is_static;
    bool is_const;
} FieldDeclNode;

typedef struct MethodDeclNode
{
    Node base;
    ObjFunction *fn;
} MethodDeclNode;

#endif // ART_CLASS_H
