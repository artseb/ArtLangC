// ============================================================
// class_ast.c — class AST constructors + free + node names
// ============================================================

#include "ast.h"
#include "class.h"

#include <stdlib.h>
#include <string.h>

Node *node_class_decl(int line, int col, ObjString *name,
                      ObjString *super_name,
                      ObjString **implements_names, int implements_count,
                      Node **fields, int field_count,
                      Node **methods, int method_count)
{
    ClassDeclNode *n = (ClassDeclNode *)ast_alloc_node(
        sizeof(ClassDeclNode), NODE_CLASS_DECL, line, col, false);

    n->name = name;
    n->superclass_name = super_name;
    n->fields = ast_copy_node_array(fields, field_count);
    n->field_count = field_count;
    n->methods = ast_copy_node_array(methods, method_count);
    n->method_count = method_count;

    if (implements_count > 0)
    {
        n->implements_names = malloc(sizeof(ObjString *) * implements_count);
        memcpy(n->implements_names, implements_names,
               sizeof(ObjString *) * implements_count);
    }
    else
    {
        n->implements_names = NULL;
    }
    n->implements_count = implements_count;

    return (Node *)n;
}

Node *node_field_decl(int line, int col, ObjString *name,
                      Node *default_value,
                      bool is_private, bool is_static, bool is_const)
{
    FieldDeclNode *n = (FieldDeclNode *)ast_alloc_node(
        sizeof(FieldDeclNode), NODE_FIELD_DECL, line, col, false);
    n->name = name;
    n->default_value = default_value;
    n->is_private = is_private;
    n->is_static = is_static;
    n->is_const = is_const;
    return (Node *)n;
}

Node *node_method_decl(int line, int col, ObjFunction *fn)
{
    MethodDeclNode *n = (MethodDeclNode *)ast_alloc_node(
        sizeof(MethodDeclNode), NODE_METHOD_DECL, line, col, false);
    n->fn = fn;
    return (Node *)n;
}

Node *node_this(int line, int col)
{
    return ast_alloc_node(sizeof(Node), NODE_THIS, line, col, true);
}

Node *node_super(int line, int col)
{
    return ast_alloc_node(sizeof(Node), NODE_SUPER, line, col, true);
}

bool class_free_children(Node *n)
{
    switch (n->type)
    {
    case NODE_CLASS_DECL:
    {
        ClassDeclNode *c = (ClassDeclNode *)n;
        for (int i = 0; i < c->field_count; i++)
            node_free_tree(&c->fields[i]);
        free(c->fields);
        for (int i = 0; i < c->method_count; i++)
            node_free_tree(&c->methods[i]);
        free(c->methods);
        free(c->implements_names);
        return true;
    }
    case NODE_FIELD_DECL:
    {
        FieldDeclNode *f = (FieldDeclNode *)n;
        if (f->default_value)
            node_free_tree(&f->default_value);
        return true;
    }
    case NODE_METHOD_DECL:
    case NODE_THIS:
    case NODE_SUPER:
        return true;
    default:
        return false;
    }
}

const char *class_node_name(NodeType t)
{
    switch (t)
    {
    case NODE_CLASS_DECL:  return "class_decl";
    case NODE_FIELD_DECL:  return "field_decl";
    case NODE_METHOD_DECL: return "method_decl";
    case NODE_THIS:        return "this";
    case NODE_SUPER:       return "super";
    default:               return NULL;
    }
}
