// ============================================================
// test_parser.c — parse source, print AST, verify node types
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "value.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"

static int g_run = 0;
static int g_failed = 0;

#define CHECK(cond, msg)                          \
    do                                            \
    {                                             \
        g_run++;                                  \
        if (!(cond))                              \
        {                                         \
            g_failed++;                           \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", \
                    __FILE__, __LINE__, (msg));   \
        }                                         \
    } while (0)

// ============================================================
// AST printer (indented tree dump)
// ============================================================

static void indent(int n)
{
    for (int i = 0; i < n; i++)
        putchar(' ');
}

static void print_node(Node *n, int depth);

static void print_block_body(BlockNode *b, int depth)
{
    for (int i = 0; i < b->count; i++)
        print_node(b->stmts[i], depth);
}

static void print_node(Node *n, int depth)
{
    if (n == NULL)
    {
        indent(depth);
        printf("(null)\n");
        return;
    }
    indent(depth);

    switch (n->type)
    {
    case NODE_LITERAL:
    {
        LiteralNode *l = (LiteralNode *)n;
        if (IS_INT(l->value))
            printf("LITERAL int %lld\n", (long long)AS_INT(l->value));
        else if (IS_FLOAT(l->value))
            printf("LITERAL float %g\n", AS_FLOAT(l->value));
        else if (IS_STRING(l->value))
        {
            char *s = obj_string_to_utf8(AS_STRING(l->value));
            printf("LITERAL string \"%s\"\n", s);
            free(s);
        }
        else if (IS_BOOL(l->value))
            printf("LITERAL bool %s\n", AS_BOOL(l->value) ? "true" : "false");
        else
            printf("LITERAL nil\n");
        break;
    }
    case NODE_VAR:
    {
        VarNode *v = (VarNode *)n;
        char *s = obj_string_to_utf8(v->name);
        printf("VAR %s\n", s);
        free(s);
        break;
    }
    case NODE_BINARY:
    {
        BinaryNode *b = (BinaryNode *)n;
        printf("BINARY %s\n", token_type_name(b->op));
        print_node(b->left, depth + 2);
        print_node(b->right, depth + 2);
        break;
    }
    case NODE_UNARY:
    {
        UnaryNode *u = (UnaryNode *)n;
        printf("UNARY %s\n", token_type_name(u->op));
        print_node(u->operand, depth + 2);
        break;
    }
    case NODE_CALL:
    {
        CallNode *c = (CallNode *)n;
        printf("CALL (%d args)\n", c->arg_count);
        print_node(c->callee, depth + 2);
        for (int i = 0; i < c->arg_count; i++)
            print_node(c->args[i], depth + 2);
        break;
    }
    case NODE_INDEX:
    {
        IndexNode *ix = (IndexNode *)n;
        printf("INDEX\n");
        print_node(ix->target, depth + 2);
        print_node(ix->index, depth + 2);
        break;
    }
    case NODE_MEMBER:
    {
        MemberNode *m = (MemberNode *)n;
        char *s = obj_string_to_utf8(m->name);
        printf("MEMBER .%s\n", s);
        free(s);
        print_node(m->target, depth + 2);
        break;
    }
    case NODE_ASSIGN:
    {
        AssignNode *a = (AssignNode *)n;
        printf("ASSIGN %s\n", token_type_name(a->op));
        print_node(a->target, depth + 2);
        print_node(a->value, depth + 2);
        break;
    }
    case NODE_DECL:
    {
        DeclNode *d = (DeclNode *)n;
        char *s = obj_string_to_utf8(d->name);
        printf("DECL %s%s\n", (d->flags & DECL_LOCAL) ? "local " : "", s);
        free(s);
        if (d->value)
            print_node(d->value, depth + 2);
        break;
    }
    case NODE_IF:
    {
        IfNode *i = (IfNode *)n;
        printf("IF\n");
        indent(depth + 2);
        printf("cond:\n");
        print_node(i->cond, depth + 4);
        indent(depth + 2);
        printf("then:\n");
        print_node(i->then_branch, depth + 4);
        if (i->else_branch)
        {
            indent(depth + 2);
            printf("else:\n");
            print_node(i->else_branch, depth + 4);
        }
        break;
    }
    case NODE_WHILE:
    {
        WhileNode *w = (WhileNode *)n;
        printf("WHILE\n");
        print_node(w->cond, depth + 2);
        print_node(w->body, depth + 2);
        break;
    }
    case NODE_FOR_RANGE:
    {
        ForRangeNode *f = (ForRangeNode *)n;
        printf("FOR_RANGE\n");
        print_node(f->init, depth + 2);
        print_node(f->end, depth + 2);
        if (f->step)
            print_node(f->step, depth + 2);
        print_node(f->body, depth + 2);
        break;
    }
    case NODE_FOR_IN:
    {
        ForInNode *f = (ForInNode *)n;
        printf("FOR_IN\n");
        if (f->key)
            print_node(f->key, depth + 2);
        print_node(f->value, depth + 2);
        print_node(f->iterable, depth + 2);
        print_node(f->body, depth + 2);
        break;
    }
    case NODE_BLOCK:
    {
        BlockNode *b = (BlockNode *)n;
        printf("BLOCK (%d)\n", b->count);
        print_block_body(b, depth + 2);
        break;
    }
    case NODE_RETURN:
    {
        ReturnNode *r = (ReturnNode *)n;
        printf("RETURN\n");
        if (r->value)
            print_node(r->value, depth + 2);
        break;
    }
    case NODE_BREAK:
        printf("BREAK\n");
        break;
    case NODE_CONTINUE:
        printf("CONTINUE\n");
        break;
    case NODE_FUN_DECL:
    {
        FunDeclNode *f = (FunDeclNode *)n;
        ObjFunction *fn = f->fn;
        if (fn->name)
        {
            char *s = obj_string_to_utf8(fn->name);
            printf("FUN_DECL %s (%d params)\n", s, fn->arity);
            free(s);
        }
        else
        {
            printf("LAMBDA (%d params)\n", fn->arity);
        }
        if (fn->body)
            print_node(fn->body, depth + 2);
        break;
    }
    case NODE_TABLE:
    {
        TableNode *t = (TableNode *)n;
        printf("TABLE (array %d, hash %d)\n", t->array_count, t->hash_count);
        for (int i = 0; i < t->array_count; i++)
        {
            print_node(t->array_items[i], depth + 2);
        }
        for (int i = 0; i < t->hash_count; i++)
        {
            indent(depth + 2);
            char *k = obj_string_to_utf8(t->hash_keys[i]);
            printf("key: %s\n", k);
            free(k);
            print_node(t->hash_values[i], depth + 4);
        }
        break;
    }
    case NODE_TYPE_COUNT:
        break;
    default:
        break;
    }
}

// ============================================================
// Helpers
// ============================================================

static Node *parse(ArtState *S, const char *src, const char *file)
{
    return parse_source(S, src, (int)strlen(src), file);
}

// ============================================================
// Tests
// ============================================================

static void test_literal(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, "42", "<test>");
    CHECK(n != NULL, "42 parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        CHECK(b->count == 1, "one statement");
        CHECK(b->stmts[0]->type == NODE_LITERAL, "is literal");
        node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_binary_precedence(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, "1 + 2 * 3", "<test>");
    CHECK(n != NULL, "parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        Node *e = b->stmts[0];
        CHECK(e->type == NODE_BINARY, "top is binary");
        CHECK(((BinaryNode *)e)->op == TOKEN_PLUS, "top op is +");
        Node *right = ((BinaryNode *)e)->right;
        CHECK(right->type == NODE_BINARY, "right is binary");
        CHECK(((BinaryNode *)right)->op == TOKEN_STAR, "right op is *");
        node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_decl(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, "local x = 5", "<test>");
    CHECK(n != NULL, "decl parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        CHECK(b->stmts[0]->type == NODE_DECL, "is decl");
        CHECK(((DeclNode *)b->stmts[0])->flags & DECL_LOCAL, "has local flag");
        node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_lamda(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, "local f = x -> x + 1", "<test>");
    CHECK(n != NULL, "lambda parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        DeclNode *d = (DeclNode *)b->stmts[0];
        CHECK(d->value->type == NODE_FUN_DECL, "value is fun_decl");
        CHECK(((FunDeclNode *)d->value)->fn->arity == 1, "lambda has 1 param");
        node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_for_range(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, "for (local i = 0 -> 10; 2) { print(i) }", "<test>");
    CHECK(n != NULL, "for-range parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        CHECK(b->stmts[0]->type == NODE_FOR_RANGE, "is for_range");
        node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_for_in(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, "for (local k, v in list) { print(k) }", "<test>");
    CHECK(n != NULL, "for-in parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        CHECK(b->stmts[0]->type == NODE_FOR_IN, "is for_in");
        node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_fun_decl(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, "fun add(a, b) { return a + b }", "<test>");
    CHECK(n != NULL, "fun decl parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        CHECK(b->stmts[0]->type == NODE_FUN_DECL, "is fun_decl");
        CHECK(((FunDeclNode *)b->stmts[0])->fn->arity == 2, "2 params");
        node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_parse_error(void)
{
    ArtState *S = art_state_new();
    parser_set_suppress(true);
    Node *n = parse(S, "if (x { }", "<test>");
    parser_set_suppress(false);
    CHECK(n == NULL, "error returns NULL");
    art_state_free(S);
}

static void test_print_ast(void)
{
    ArtState *S = art_state_new();
    const char *src =
        "fun square(n) {\n"
        "    return n * n\n"
        "}\n";
    Node *n = parse(S, src, "<test>");
    printf("\n--- AST for a simple function ---\n");
    if (n)
        print_node(n, 0);
    printf("\n");
    if (n)
        node_free_tree(&n);
    art_state_free(S);
}

// ============================================================
// Entry point
// ============================================================

int main(void)
{
    printf("ART parser tests\n");
    printf("================\n");

    // MUST call this before any parse — it registers the statement
    // parsers. Failing to call it makes the dispatch table empty
    // and every statement becomes an expression statement.
    parser_init(NULL);

    test_literal();
    test_binary_precedence();
    test_decl();
    test_lamda();
    test_for_range();
    test_for_in();
    test_fun_decl();
    test_parse_error();
    test_print_ast();

    printf("\n%d checks, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}