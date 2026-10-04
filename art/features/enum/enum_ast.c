// ============================================================
// enum_ast.c — enum AST constructors + free + node names
// ============================================================

#include "enum.h"

#include <stdlib.h>

Node *node_enum_decl(int line, int col, ObjString *name,
                     Node **members, int member_count)
{
    EnumDeclNode *n = (EnumDeclNode *)ast_alloc_node(
        sizeof(EnumDeclNode), NODE_ENUM_DECL, line, col, false);
    n->name = name;
    n->members = ast_copy_node_array(members, member_count);
    n->member_count = member_count;
    return (Node *)n;
}

Node *node_enum_member(int line, int col, ObjString *name, Node *value)
{
    EnumMemberNode *n = (EnumMemberNode *)ast_alloc_node(
        sizeof(EnumMemberNode), NODE_ENUM_MEMBER, line, col, false);
    n->name = name;
    n->value = value;
    return (Node *)n;
}

bool enum_free_children(Node *n)
{
    switch (n->type)
    {
    case NODE_ENUM_DECL:
    {
        EnumDeclNode *e = (EnumDeclNode *)n;
        for (int i = 0; i < e->member_count; i++)
            node_free_tree(&e->members[i]);
        free(e->members);
        return true;
    }
    case NODE_ENUM_MEMBER:
    {
        EnumMemberNode *m = (EnumMemberNode *)n;
        if (m->value)
            node_free_tree(&m->value);
        return true;
    }
    default:
        return false;
    }
}

const char *enum_node_name(NodeType t)
{
    switch (t)
    {
    case NODE_ENUM_DECL:   return "enum_decl";
    case NODE_ENUM_MEMBER: return "enum_member";
    default:               return NULL;
    }
}
