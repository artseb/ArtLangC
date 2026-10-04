#ifndef ART_ENUM_H
#define ART_ENUM_H

#include "ast.h"
#include "value.h"
#include "state.h"

typedef struct Parser Parser;

Node *enum_parse(Parser *P);

Node *node_enum_decl(int line, int col, ObjString *name,
                     Node **members, int member_count);
Node *node_enum_member(int line, int col, ObjString *name, Node *value);

ObjEnum *obj_enum_new(ArtState *S, ObjString *name);
ObjEnumValue *obj_enum_value_new(ArtState *S, ObjEnum *parent,
                                 ObjString *name, Value value);

Value eval_enum_decl(ArtState *S, Node *n);

Value enum_get_member(ArtState *S, ObjEnum *e, ObjString *name, Node *at);
Value enum_value_get_member(ArtState *S, ObjEnumValue *ev,
                            ObjString *name, Node *at);

bool enum_free_children(Node *n);
const char *enum_node_name(NodeType t);
bool enum_gc_blacken(ArtState *S, Obj *o);
bool enum_gc_free_buffers(ArtState *S, Obj *o);

typedef struct Feature Feature;
extern Feature enum_feature;

typedef struct EnumMemberNode
{
    Node base;
    ObjString *name;
    Node *value;
} EnumMemberNode;

typedef struct EnumDeclNode
{
    Node base;
    ObjString *name;
    Node **members;
    int member_count;
} EnumDeclNode;

#endif
