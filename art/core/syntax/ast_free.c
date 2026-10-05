// ============================================================
// ast_free.c — recursive free walk + type names
//
// Called from GC's free_object_buffers when an ObjFunction is
// swept. Frees every node in the function's tree.
//
// IMPORTANT: this must free NODES, not the objects they point at.
// ObjStrings are interned (GC-managed, live in S->strings).
// ObjFunctions are GC-managed and freed by the GC itself.
// ============================================================

#include <stdlib.h>
#include "ast.h"
#include "features/features.h"

static void free_children(Node *n)
{
    FEATURES_FOR_EACH(f)
    {
        if (f->free_children && f->free_children(n))
            return;
    }

    switch (n->type)
    {
    case NODE_BINARY:
    {
        BinaryNode *b = (BinaryNode *)n;
        node_free_tree(&b->left);
        node_free_tree(&b->right);
        break;
    }
    case NODE_UNARY:
    {
        UnaryNode *u = (UnaryNode *)n;
        node_free_tree(&u->operand);
        break;
    }
    case NODE_CALL:
    {
        CallNode *c = (CallNode *)n;
        node_free_tree(&c->callee);
        for (int i = 0; i < c->arg_count; i++)
        {
            node_free_tree(&c->args[i]);
        }
        free(c->args);
        break;
    }
    case NODE_INDEX:
    {
        IndexNode *ix = (IndexNode *)n;
        node_free_tree(&ix->target);
        node_free_tree(&ix->index);
        break;
    }
    case NODE_MEMBER:
    {
        MemberNode *m = (MemberNode *)n;
        node_free_tree(&m->target);
        break;
    }
    case NODE_ASSIGN:
    {
        AssignNode *a = (AssignNode *)n;
        node_free_tree(&a->target);
        node_free_tree(&a->value);
        break;
    }
    case NODE_DECL:
    {
        DeclNode *d = (DeclNode *)n;
        node_free_tree(&d->value);
        break;
    }
    case NODE_IF:
    {
        IfNode *i = (IfNode *)n;
        node_free_tree(&i->cond);
        node_free_tree(&i->then_branch);
        node_free_tree(&i->else_branch);
        break;
    }
    case NODE_WHILE:
    {
        WhileNode *w = (WhileNode *)n;
        node_free_tree(&w->cond);
        node_free_tree(&w->body);
        break;
    }
    case NODE_FOR_RANGE:
    {
        ForRangeNode *f = (ForRangeNode *)n;
        node_free_tree(&f->init);
        node_free_tree(&f->end);
        node_free_tree(&f->step);
        node_free_tree(&f->body);
        break;
    }
    case NODE_FOR_IN:
    {
        ForInNode *f = (ForInNode *)n;
        node_free_tree(&f->key);
        node_free_tree(&f->value);
        node_free_tree(&f->iterable);
        node_free_tree(&f->body);
        break;
    }
    case NODE_BLOCK:
    {
        BlockNode *b = (BlockNode *)n;
        for (int i = 0; i < b->count; i++)
        {
            node_free_tree(&b->stmts[i]);
        }
        free(b->stmts);
        break;
    }
    case NODE_RETURN:
    {
        ReturnNode *r = (ReturnNode *)n;
        node_free_tree(&r->value);
        break;
    }
    case NODE_TABLE:
    {
        TableNode *t = (TableNode *)n;
        for (int i = 0; i < t->array_count; i++)
        {
            node_free_tree(&t->array_items[i]);
        }
        free(t->array_items);

        for (int i = 0; i < t->hash_count; i++)
        {
            node_free_tree(&t->hash_values[i]);
        }
        free(t->hash_keys);
        free(t->hash_values);
        break;
    }
    case NODE_INTERP:
    {
        InterpNode *t = (InterpNode *)n;
        for (int i = 0; i < t->part_count; i++)
            node_free_tree(&t->parts[i]);
        free(t->parts);
        // specs are interned ObjStrings, GC-managed. Only the
        // parallel array itself needs freeing.
        free(t->specs);
        break;
    }
    case NODE_MULTI_DECL:
    {
        MultiDeclNode *m = (MultiDeclNode *)n;
        free(m->names);
        if (m->value)
            node_free_tree(&m->value);
        break;
    }
    case NODE_IS:
    {
        IsNode *in = (IsNode *)n;
        node_free_tree(&in->left);
        break;
    }
    case NODE_LITERAL:
    case NODE_VAR:
    case NODE_BREAK:
    case NODE_CONTINUE:
    case NODE_FUN_DECL:
    case NODE_TYPE_COUNT:
    default:
        break;
    }
}

void node_free_tree(Node **n)
{
    if (n == NULL || *n == NULL)
        return;

    Node *node = *n;
    *n = NULL;

    free_children(node);
    free(node);
}

const char *node_type_name(NodeType t)
{
    FEATURES_FOR_EACH(f)
    {
        if (f->node_name)
        {
            const char *n = f->node_name(t);
            if (n)
                return n;
        }
    }

    switch (t)
    {
    case NODE_LITERAL:    return "literal";
    case NODE_VAR:        return "variable";
    case NODE_BINARY:     return "binary expression";
    case NODE_UNARY:      return "unary expression";
    case NODE_CALL:       return "call";
    case NODE_INDEX:      return "index";
    case NODE_MEMBER:     return "member access";
    case NODE_ASSIGN:     return "assignment";
    case NODE_DECL:       return "declaration";
    case NODE_IF:         return "if";
    case NODE_WHILE:      return "while";
    case NODE_FOR_RANGE:  return "for range";
    case NODE_FOR_IN:     return "for in";
    case NODE_BLOCK:      return "block";
    case NODE_RETURN:     return "return";
    case NODE_BREAK:      return "break";
    case NODE_CONTINUE:   return "continue";
    case NODE_FUN_DECL:   return "function declaration";
    case NODE_TABLE:      return "table literal";
    case NODE_INTERP:     return "interp";
    case NODE_MULTI_DECL: return "multi_decl";
    case NODE_IS:         return "is";
    case NODE_TYPE_COUNT: return "<invalid>";
    default:              return "nil";
    }
    return "unknown";
}
