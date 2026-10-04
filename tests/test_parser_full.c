// ============================================================
// test_parser_full.c — parse a realistic program end to end
//
// Covers every core construct the parser supports today.
// Feature-dependent syntax (class, enum, switch, import) is
// excluded — those get their own tests once ported.
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

static Node *parse(ArtState *S, const char *src)
{
    return parse_source(S, src, (int)strlen(src), "<test>");
}

// ============================================================
// Realistic program — battle-style, no class/enum/switch
// ============================================================

static const char *k_program =
    "// a small program exercising every core construct\n"
    "local total = 0\n"
    "local log = []\n"
    "\n"
    "fun add(a, b) {\n"
    "    return a + b\n"
    "}\n"
    "\n"
    "fun clamp(v, lo, hi) {\n"
    "    if (v < lo) {\n"
    "        return lo\n"
    "    } else if (v > hi) {\n"
    "        return hi\n"
    "    } else {\n"
    "        return v\n"
    "    }\n"
    "}\n"
    "\n"
    "fun makeLogger(label) {\n"
    "    local turn = 0\n"
    "    return fun(message) {\n"
    "        turn = turn + 1\n"
    "        return label + \" turn \" + turn + \": \" + message\n"
    "    }\n"
    "}\n"
    "\n"
    "local heroLog = makeLogger(\"Hero\")\n"
    "local monsterLog = makeLogger(\"Monster\")\n"
    "\n"
    "for (local i = 0 -> 10; 2) {\n"
    "    if (i % 3 == 0) {\n"
    "        continue\n"
    "    }\n"
    "    total = total + i\n"
    "}\n"
    "\n"
    "local dropTable = [\"Ember Shard\", \"Waterlogged Boot\", \"Small Gem\"]\n"
    "local inventory = [\"potions\" = 3, \"gold\" = 42]\n"
    "\n"
    "for (local index, item in dropTable) {\n"
    "    log.push(item)\n"
    "}\n"
    "\n"
    "local f = x -> x * 2\n"
    "local g = (a, b) -> a + b\n"
    "local h = () -> print(\"hello\")\n"
    "\n"
    "local queue = [1, 2, 3]\n"
    "queue.push(4)\n"
    "queue.reverse()\n"
    "\n"
    "while (total < 100) {\n"
    "    total = total + 1\n"
    "    if (total > 50) {\n"
    "        break\n"
    "    }\n"
    "}\n"
    "\n"
    "print(heroLog(\"wins\"))\n"
    "print(\"Total: \" + total)\n";

// ============================================================
// Walk the AST, count nodes by type — verifies structure
// without asserting on every detail
// ============================================================

typedef struct
{
    int fun_decls;
    int lambdas;
    int for_ranges;
    int for_ins;
    int whiles;
    int ifs;
    int returns;
    int breaks;
    int continues;
    int decls;
    int calls;
    int binops;
} NodeStats;

static void count_nodes(Node *n, NodeStats *s)
{
    if (n == NULL)
        return;

    switch (n->type)
    {
    case NODE_FUN_DECL:
    {
        FunDeclNode *f = (FunDeclNode *)n;
        if (f->fn->name)
            s->fun_decls++;
        else
            s->lambdas++;
        count_nodes(f->fn->body, s);
        return;
    }
    case NODE_FOR_RANGE:
    {
        ForRangeNode *f = (ForRangeNode *)n;
        s->for_ranges++;
        count_nodes(f->init, s);
        count_nodes(f->end, s);
        count_nodes(f->step, s);
        count_nodes(f->body, s);
        return;
    }
    case NODE_FOR_IN:
    {
        ForInNode *f = (ForInNode *)n;
        s->for_ins++;
        count_nodes(f->key, s);
        count_nodes(f->value, s);
        count_nodes(f->iterable, s);
        count_nodes(f->body, s);
        return;
    }
    case NODE_WHILE:
    {
        WhileNode *w = (WhileNode *)n;
        s->whiles++;
        count_nodes(w->cond, s);
        count_nodes(w->body, s);
        return;
    }
    case NODE_IF:
    {
        IfNode *i = (IfNode *)n;
        s->ifs++;
        count_nodes(i->cond, s);
        count_nodes(i->then_branch, s);
        count_nodes(i->else_branch, s);
        return;
    }
    case NODE_RETURN:
    {
        ReturnNode *r = (ReturnNode *)n;
        s->returns++;
        count_nodes(r->value, s);
        return;
    }
    case NODE_BREAK:
        s->breaks++;
        return;
    case NODE_CONTINUE:
        s->continues++;
        return;
    case NODE_DECL:
    {
        DeclNode *d = (DeclNode *)n;
        s->decls++;
        count_nodes(d->value, s);
        return;
    }
    case NODE_CALL:
    {
        CallNode *c = (CallNode *)n;
        s->calls++;
        count_nodes(c->callee, s);
        for (int i = 0; i < c->arg_count; i++)
            count_nodes(c->args[i], s);
        return;
    }
    case NODE_BINARY:
    {
        BinaryNode *b = (BinaryNode *)n;
        s->binops++;
        count_nodes(b->left, s);
        count_nodes(b->right, s);
        return;
    }
    case NODE_BLOCK:
    {
        BlockNode *b = (BlockNode *)n;
        for (int i = 0; i < b->count; i++)
            count_nodes(b->stmts[i], s);
        return;
    }
    case NODE_ASSIGN:
    {
        AssignNode *a = (AssignNode *)n;
        count_nodes(a->target, s);
        count_nodes(a->value, s);
        return;
    }
    case NODE_INDEX:
    {
        IndexNode *ix = (IndexNode *)n;
        count_nodes(ix->target, s);
        count_nodes(ix->index, s);
        return;
    }
    case NODE_MEMBER:
    {
        MemberNode *m = (MemberNode *)n;
        count_nodes(m->target, s);
        return;
    }
    case NODE_UNARY:
    {
        UnaryNode *u = (UnaryNode *)n;
        count_nodes(u->operand, s);
        return;
    }
    case NODE_TABLE:
    {
        TableNode *t = (TableNode *)n;
        for (int i = 0; i < t->array_count; i++)
        {
            count_nodes(t->array_items[i], s);
        }
        for (int i = 0; i < t->hash_count; i++)
        {
            count_nodes(t->hash_values[i], s);
        }
        return;
    }
    default:
        return;
    }
}

// ============================================================
// Tests
// ============================================================

static void test_full_program(void)
{
    ArtState *S = art_state_new();
    Node *n = parse(S, k_program);

    CHECK(n != NULL, "full program parses without errors");

    if (n != NULL)
    {
        NodeStats s = {0};
        count_nodes(n, &s);

        printf("stats: fun_decls=%d lambdas=%d for_ranges=%d for_ins=%d "
               "whiles=%d ifs=%d returns=%d breaks=%d continues=%d "
               "decls=%d calls=%d binops=%d\n",
               s.fun_decls, s.lambdas, s.for_ranges, s.for_ins,
               s.whiles, s.ifs, s.returns, s.breaks, s.continues,
               s.decls, s.calls, s.binops);

        // Program contains exactly these counts
        CHECK(s.fun_decls == 3, "3 named functions");
        CHECK(s.lambdas == 4, "4 lambdas (logger + f + g + h)");
        CHECK(s.for_ranges == 1, "1 for-range loop");
        CHECK(s.for_ins == 1, "1 for-in loop");
        CHECK(s.whiles == 1, "1 while loop");
        CHECK(s.ifs == 4, "4 ifs (clamp x2 + loop checks x2)");
        CHECK(s.returns >= 4, "at least 4 returns");
        CHECK(s.breaks == 1, "1 break");
        CHECK(s.continues == 1, "1 continue");
        CHECK(s.decls >= 10, "at least 10 declarations");
        CHECK(s.calls >= 5, "at least 5 calls");

        node_free_tree(&n);
    }

    art_state_free(S);
}

// ============================================================
// Individual constructs
// ============================================================

static void test_assignment_ops(void)
{
    ArtState *S = art_state_new();

    const char *cases[] = {
        "x = 5",
        "x += 1",
        "x -= 1",
        "x *= 2",
        "x /= 2",
        "x %= 3",
        "x ^= 2",
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        Node *n = parse(S, cases[i]);
        CHECK(n != NULL, cases[i]);
        if (n)
            node_free_tree(&n);
    }
    art_state_free(S);
}

static void test_member_and_index(void)
{
    ArtState *S = art_state_new();

    Node *n = parse(S, "obj.field.sub[1][2]");
    CHECK(n != NULL, "nested member + index parses");
    if (n)
        node_free_tree(&n);

    n = parse(S, "obj.field = 5");
    CHECK(n != NULL, "member assign parses");
    if (n)
        node_free_tree(&n);

    n = parse(S, "list[0] = 5");
    CHECK(n != NULL, "index assign parses");
    if (n)
        node_free_tree(&n);

    art_state_free(S);
}

static void test_and_or_chains(void)
{
    ArtState *S = art_state_new();

    // Short-circuit chains — parser just builds binaries
    Node *n = parse(S, "a and b and c or d");
    CHECK(n != NULL, "and/or chain parses");
    if (n)
        node_free_tree(&n);

    // Precedence: or binds looser than and
    n = parse(S, "true or false and false");
    CHECK(n != NULL, "or/and precedence parses");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        BinaryNode *top = (BinaryNode *)b->stmts[0];
        CHECK(top->op == TOKEN_OR, "top level is OR (correct precedence)");
        CHECK(top->right->type == NODE_BINARY, "right side is a binary");
        CHECK(((BinaryNode *)top->right)->op == TOKEN_AND,
              "right side is AND");
        node_free_tree(&n);
    }

    art_state_free(S);
}

static void test_nested_lambdas(void)
{
    ArtState *S = art_state_new();

    Node *n = parse(S, "local f = x -> y -> x + y");
    CHECK(n != NULL, "curried lambda parses");
    if (n)
        node_free_tree(&n);

    n = parse(S, "list.map(item -> item.name)");
    CHECK(n != NULL, "inline lambda call parses");
    if (n)
        node_free_tree(&n);

    n = parse(S, "list.reduce((sum, x) -> sum + x, 0)");
    CHECK(n != NULL, "multi-param lambda with comma-parsed arg parses");
    if (n)
        node_free_tree(&n);

    art_state_free(S);
}

static void test_grouping_vs_lambda(void)
{
    ArtState *S = art_state_new();

    // (x) without -> is grouping, not lambda
    Node *n = parse(S, "local y = (x)");
    CHECK(n != NULL, "(x) is grouping");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        DeclNode *d = (DeclNode *)b->stmts[0];
        CHECK(d->value->type == NODE_VAR, "grouping yields VAR node");
        node_free_tree(&n);
    }

    // (x) -> is lambda
    n = parse(S, "local y = (x) -> x");
    CHECK(n != NULL, "(x) -> is lambda");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        DeclNode *d = (DeclNode *)b->stmts[0];
        CHECK(d->value->type == NODE_FUN_DECL, "lambda yields FUN_DECL");
        node_free_tree(&n);
    }

    art_state_free(S);
}

static void test_empty_block(void)
{
    ArtState *S = art_state_new();

    Node *n = parse(S, "if (true) {}");
    CHECK(n != NULL, "empty block parses");
    if (n)
        node_free_tree(&n);

    n = parse(S, "fun nothing() {}");
    CHECK(n != NULL, "empty function body parses");
    if (n)
        node_free_tree(&n);

    art_state_free(S);
}

static void test_semicolons_optional(void)
{
    ArtState *S = art_state_new();

    Node *n = parse(S, "local x = 5; local y = 10; print(x + y)");
    CHECK(n != NULL, "semicolons parse");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        CHECK(b->count == 3, "3 statements with semicolons");
        node_free_tree(&n);
    }

    n = parse(S, "local x = 5\nlocal y = 10\nprint(x + y)");
    CHECK(n != NULL, "newlines separate statements");
    if (n)
    {
        BlockNode *b = (BlockNode *)n;
        CHECK(b->count == 3, "3 statements without semicolons");
        node_free_tree(&n);
    }

    art_state_free(S);
}

static void test_multiple_errors_reported(void)
{
    ArtState *S = art_state_new();
    // Deliberately broken, should not crash, should return NULL
    parser_set_suppress(true);
    Node *n = parse(S, "if (x { } if (y { }");
    parser_set_suppress(false);
    CHECK(n == NULL, "multiple broken ifs -> NULL, no crash");
    art_state_free(S);
}

// ============================================================
// Entry
// ============================================================

int main(void)
{
    printf("ART parser full-program tests\n");
    printf("=============================\n");

    parser_init(NULL);

    test_full_program();
    test_assignment_ops();
    test_member_and_index();
    test_and_or_chains();
    test_nested_lambdas();
    test_grouping_vs_lambda();
    test_empty_block();
    test_semicolons_optional();
    test_multiple_errors_reported();

    printf("\n%d checks, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}