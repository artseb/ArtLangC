#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"
#include "ast.h"
#include "parser.h"

// The class feature's node structs (ClassDeclNode, FieldDeclNode,
// MethodDeclNode) live in features/class/class.h. Unlike the other
// tests, this one reaches into them directly, so it needs the
// include. test_class_eval.c gets it transitively via registry.h,
// which re-exports features/registry.h.
#include "features/class/class.h"

static int checks = 0;
static int failed = 0;

static void check(int cond, const char *msg)
{
    checks++;
    if (!cond)
    {
        failed++;
        fprintf(stderr, "FAIL: %s\n", msg);
    }
}

static const char *SRC =
    "class Vector3 extends Vector2 {\n"
    "    local z = 0\n"
    "    fun Vector3(x: Number = 0, y: Number = 0, z: Number = 0) {\n"
    "        super(x, y)\n"
    "        this.z = z\n"
    "    }\n"
    "    static fun one() { return Vector3(1, 1, 1) }\n"
    "    operator +(other) {\n"
    "        return Vector3(this.x + other.x, this.y + other.y, this.z + other.z)\n"
    "    }\n"
    "    get magnitude() {\n"
    "        return 0\n"
    "    }\n"
    "}\n";

int main(void)
{
    printf("ART class-parse tests\n");
    printf("=====================\n\n");
    fflush(stdout);

    printf("[1] art_state_new\n");
    fflush(stdout);
    ArtState *S = art_state_new();

    printf("[2] parse_source\n");
    fflush(stdout);
    Node *prog = parse_source(S, SRC, (int)strlen(SRC), "<class>");
    printf("[3] parse returned %p\n", (void *)prog);
    fflush(stdout);

    check(prog != NULL, "parse returned non-NULL");
    if (prog == NULL)
    {
        printf("[x] bailing, parse failed\n");
        art_state_free(S);
        printf("\n%d checks, %d failed\n", checks, failed);
        return 1;
    }

    printf("[4] checking top-level block\n");
    fflush(stdout);
    check(prog->type == NODE_BLOCK, "top-level is a block");
    BlockNode *b = (BlockNode *)prog;
    printf("[5] block count = %d\n", b->count);
    fflush(stdout);
    check(b->count == 1, "one statement at top level");
    check(b->stmts[0]->type == NODE_CLASS_DECL, "statement is a CLASS_DECL");

    printf("[6] reading class decl\n");
    fflush(stdout);
    ClassDeclNode *c = (ClassDeclNode *)b->stmts[0];
    printf("    name ptr = %p\n", (void *)c->name);
    fflush(stdout);
    printf("    super ptr = %p\n", (void *)c->superclass_name);
    fflush(stdout);
    printf("    fields = %d, methods = %d\n", c->field_count, c->method_count);
    fflush(stdout);

    check(c->name != NULL &&
              strcmp(obj_string_to_utf8(c->name), "Vector3") == 0,
          "class name is Vector3");
    check(c->superclass_name != NULL &&
              strcmp(obj_string_to_utf8(c->superclass_name), "Vector2") == 0,
          "superclass is Vector2");

    printf("[7] checking fields\n");
    fflush(stdout);
    check(c->field_count == 1, "one field");
    if (c->field_count == 1)
    {
        printf("    fields[0] = %p\n", (void *)c->fields[0]);
        fflush(stdout);
        FieldDeclNode *f = (FieldDeclNode *)c->fields[0];
        check(f->base.type == NODE_FIELD_DECL, "field is FIELD_DECL");
        check(f->name && strcmp(obj_string_to_utf8(f->name), "z") == 0, "field is z");
        check(f->is_private, "field z is private (local)");
    }

    printf("[8] checking methods\n");
    fflush(stdout);
    check(c->method_count == 4, "four methods (ctor, one, +, getter)");
    if (c->method_count == 4)
    {
        for (int i = 0; i < 4; i++)
        {
            MethodDeclNode *m = (MethodDeclNode *)c->methods[i];
            printf("    method[%d] = %p, fn = %p, name = %p\n",
                   i, (void *)m, (void *)m->fn, (void *)m->fn->name);
            fflush(stdout);
            if (m->fn->name)
                printf("        name = '%s', arity = %d\n",
                       obj_string_to_utf8(m->fn->name), m->fn->arity);
            else
                printf("        (unnamed), arity = %d, is_op = %d\n",
                       m->fn->arity, m->fn->is_operator);
            fflush(stdout);
        }
    }

    printf("[9] node_free_tree\n");
    fflush(stdout);
    node_free_tree(&prog);

    printf("[10] art_state_free\n");
    fflush(stdout);
    art_state_free(S);

    printf("[11] done\n");
    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}