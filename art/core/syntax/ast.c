// ============================================================
// ast.c — node constructors
// ============================================================

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "features/features.h"

Node *ast_alloc_node(size_t size, NodeType type,
                            int line, int col, bool is_expr)
{
    Node *n = malloc(size);
    if (!n)
    {
        fprintf(stderr, "art: out of memory allocating AST node\n");
        abort();
    }
    memset(n, 0, size);
    n->type = type;
    n->line = line;
    n->column = col;
    n->is_expression = is_expr;
    return n;
}

Node **ast_copy_node_array(Node **src, int count)
{
    if (count == 0)
        return NULL;
    Node **dst = malloc(sizeof(Node *) * count);
    if (!dst)
    {
        fprintf(stderr, "art: out of memory allocating node array\n");
        abort();
    }
    memcpy(dst, src, sizeof(Node *) * count);
    return dst;
}

Node *node_literal(int line, int col, Value value)
{
    LiteralNode *n = (LiteralNode *)ast_alloc_node(
        sizeof(LiteralNode), NODE_LITERAL, line, col, true);
    n->value = value;
    return (Node *)n;
}

Node *node_var(int line, int col, ObjString *name)
{
    VarNode *n = (VarNode *)ast_alloc_node(
        sizeof(VarNode), NODE_VAR, line, col, true);
    n->name = name;
    return (Node *)n;
}

Node *node_binary(int line, int col, Node *left, TokenType op, Node *right)
{
    BinaryNode *n = (BinaryNode *)ast_alloc_node(
        sizeof(BinaryNode), NODE_BINARY, line, col, true);
    n->left = left;
    n->right = right;
    n->op = op;
    return (Node *)n;
}

Node *node_unary(int line, int col, TokenType op, Node *operand)
{
    UnaryNode *n = (UnaryNode *)ast_alloc_node(
        sizeof(UnaryNode), NODE_UNARY, line, col, true);
    n->operand = operand;
    n->op = op;
    return (Node *)n;
}

Node *node_is(int line, int col, Node *left, ObjString *type_name)
{
    IsNode *n = (IsNode *)ast_alloc_node(
        sizeof(IsNode), NODE_IS, line, col, true);
    n->left = left;
    n->type_name = type_name;
    return (Node *)n;
}

Node *node_call(int line, int col, Node *callee, Node **args, int arg_count)
{
    CallNode *n = (CallNode *)ast_alloc_node(
        sizeof(CallNode), NODE_CALL, line, col, true);
    n->callee = callee;
    n->args = ast_copy_node_array(args, arg_count);
    n->arg_count = arg_count;
    return (Node *)n;
}

Node *node_index(int line, int col, Node *target, Node *index)
{
    IndexNode *n = (IndexNode *)ast_alloc_node(
        sizeof(IndexNode), NODE_INDEX, line, col, true);
    n->target = target;
    n->index = index;
    return (Node *)n;
}

Node *node_member(int line, int col, Node *target, ObjString *name)
{
    MemberNode *n = (MemberNode *)ast_alloc_node(
        sizeof(MemberNode), NODE_MEMBER, line, col, true);
    n->target = target;
    n->name = name;
    return (Node *)n;
}

Node *node_assign(int line, int col, Node *target, TokenType op, Node *value)
{
    AssignNode *n = (AssignNode *)ast_alloc_node(
        sizeof(AssignNode), NODE_ASSIGN, line, col, true);
    n->target = target;
    n->value = value;
    n->op = op;
    return (Node *)n;
}

Node *node_decl(int line, int col, ObjString *name, Node *value, uint32_t flags)
{
    DeclNode *n = (DeclNode *)ast_alloc_node(
        sizeof(DeclNode), NODE_DECL, line, col, false);
    n->name = name;
    n->value = value;
    n->flags = flags;
    return (Node *)n;
}

Node *node_if(int line, int col, Node *cond, Node *then_b, Node *else_b)
{
    IfNode *n = (IfNode *)ast_alloc_node(
        sizeof(IfNode), NODE_IF, line, col, false);
    n->cond = cond;
    n->then_branch = then_b;
    n->else_branch = else_b;
    return (Node *)n;
}

Node *node_while(int line, int col, Node *cond, Node *body)
{
    WhileNode *n = (WhileNode *)ast_alloc_node(
        sizeof(WhileNode), NODE_WHILE, line, col, false);
    n->cond = cond;
    n->body = body;
    return (Node *)n;
}

Node *node_for_range(int line, int col, Node *init, Node *end, Node *step, Node *body)
{
    ForRangeNode *n = (ForRangeNode *)ast_alloc_node(
        sizeof(ForRangeNode), NODE_FOR_RANGE, line, col, false);
    n->init = init;
    n->end = end;
    n->step = step;
    n->body = body;
    return (Node *)n;
}

Node *node_for_in(int line, int col, Node *key, Node *value,
                  Node *iterable, Node *body)
{
    ForInNode *n = (ForInNode *)ast_alloc_node(
        sizeof(ForInNode), NODE_FOR_IN, line, col, false);
    n->key = key;
    n->value = value;
    n->iterable = iterable;
    n->body = body;
    return (Node *)n;
}

Node *node_block(int line, int col, Node **stmts, int count)
{
    BlockNode *n = (BlockNode *)ast_alloc_node(
        sizeof(BlockNode), NODE_BLOCK, line, col, false);
    n->stmts = ast_copy_node_array(stmts, count);
    n->count = count;
    return (Node *)n;
}

Node *node_return(int line, int col, Node *value)
{
    ReturnNode *n = (ReturnNode *)ast_alloc_node(
        sizeof(ReturnNode), NODE_RETURN, line, col, false);
    n->value = value;
    return (Node *)n;
}

Node *node_break(int line, int col)
{
    return ast_alloc_node(sizeof(Node), NODE_BREAK, line, col, false);
}

Node *node_continue(int line, int col)
{
    return ast_alloc_node(sizeof(Node), NODE_CONTINUE, line, col, false);
}

Node *node_fun_decl(int line, int col, ObjFunction *fn)
{
    FunDeclNode *n = (FunDeclNode *)ast_alloc_node(
        sizeof(FunDeclNode), NODE_FUN_DECL, line, col, true);
    n->fn = fn;
    return (Node *)n;
}

Node *node_table(int line, int col,
                 Node **array_items, int array_count,
                 ObjString **hash_keys, Node **hash_values, int hash_count)
{
    TableNode *n = (TableNode *)ast_alloc_node(
        sizeof(TableNode), NODE_TABLE, line, col, true);

    n->array_items = ast_copy_node_array(array_items, array_count);
    n->array_count = array_count;

    if (hash_count > 0)
    {
        n->hash_keys = malloc(sizeof(ObjString *) * hash_count);
        n->hash_values = malloc(sizeof(Node *) * hash_count);
        memcpy(n->hash_keys, hash_keys, sizeof(ObjString *) * hash_count);
        memcpy(n->hash_values, hash_values, sizeof(Node *) * hash_count);
    }
    else
    {
        n->hash_keys = NULL;
        n->hash_values = NULL;
    }
    n->hash_count = hash_count;

    return (Node *)n;
}

Node *node_interp(int line, int col, Node **parts, int count)
{
    InterpNode *n = (InterpNode *)ast_alloc_node(
        sizeof(InterpNode), NODE_INTERP, line, col, true);
    n->parts = ast_copy_node_array(parts, count);
    n->part_count = count;
    return (Node *)n;
}

Node *node_multi_decl(int line, int col, ObjString **names, int name_count,
                      Node *value, uint32_t flags)
{
    MultiDeclNode *n = (MultiDeclNode *)ast_alloc_node(
        sizeof(MultiDeclNode), NODE_MULTI_DECL, line, col, false);

    n->names = malloc(sizeof(ObjString *) * name_count);
    memcpy(n->names, names, sizeof(ObjString *) * name_count);
    n->name_count = name_count;
    n->value = value;
    n->flags = flags;
    return (Node *)n;
}
