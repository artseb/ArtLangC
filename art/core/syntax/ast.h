#ifndef ART_AST_H
#define ART_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "value.h"
#include "token.h"

typedef struct Node Node;
typedef struct ObjFunction ObjFunction;

#define CORE_NODES(X)         \
    X(LITERAL)                \
    X(VAR)                    \
    X(BINARY)                 \
    X(UNARY)                  \
    X(CALL)                   \
    X(INDEX)                  \
    X(MEMBER)                 \
    X(ASSIGN)                 \
    X(DECL)                   \
    X(IF)                     \
    X(WHILE)                  \
    X(FOR_RANGE)              \
    X(FOR_IN)                 \
    X(BLOCK)                  \
    X(RETURN)                 \
    X(BREAK)                  \
    X(CONTINUE)               \
    X(FUN_DECL)               \
    X(TABLE)                  \
    X(INTERP)                 \
    X(MULTI_DECL)             \
    X(IS)

// --- feature node lists (kept here so the full node set is in one place) ---

#define CLASS_NODES(X) \
    X(CLASS_DECL)      \
    X(FIELD_DECL)      \
    X(METHOD_DECL)     \
    X(THIS)            \
    X(SUPER)

#define ENUM_NODES(X) \
    X(ENUM_DECL)      \
    X(ENUM_MEMBER)

#define SWITCH_NODES(X) \
    X(SWITCH)

#define INTERFACE_NODES(X) \
    X(INTERFACE_DECL)

#define ALL_NODES(X) \
    CORE_NODES(X)    \
    CLASS_NODES(X)   \
    ENUM_NODES(X)    \
    SWITCH_NODES(X)  \
    INTERFACE_NODES(X)

typedef enum
{
#define X(name) NODE_##name,
    ALL_NODES(X)
#undef X
    NODE_TYPE_COUNT
} NodeType;

// DECL_LOCAL  : `local x = ...`
// DECL_CONST  : `local const x = ...` or top-level `const x = ...`
// DECL_ASSIGN : `a, b = expr` — multi-assignment, not declaration.
//               Set on MultiDeclNode only, produced at statement
//               level when the leading token is an identifier.
#define DECL_LOCAL  (1u << 0)
#define DECL_CONST  (1u << 1)
#define DECL_ASSIGN (1u << 2)

struct Node
{
    NodeType type;
    int line;
    int column;
    bool is_expression;
};

typedef struct LiteralNode
{
    Node base;
    Value value;
} LiteralNode;

typedef struct VarNode
{
    Node base;
    ObjString *name;
} VarNode;

typedef struct BinaryNode
{
    Node base;
    Node *left;
    Node *right;
    TokenType op;
} BinaryNode;

typedef struct UnaryNode
{
    Node base;
    Node *operand;
    TokenType op;
} UnaryNode;

typedef struct IsNode
{
    Node base;
    Node *left;
    ObjString *type_name;
} IsNode;

typedef struct CallNode
{
    Node base;
    Node *callee;
    Node **args;
    int arg_count;
} CallNode;

typedef struct IndexNode
{
    Node base;
    Node *target;
    Node *index;
} IndexNode;

typedef struct MemberNode
{
    Node base;
    Node *target;
    ObjString *name;
} MemberNode;

typedef struct AssignNode
{
    Node base;
    Node *target;
    Node *value;
    TokenType op;
} AssignNode;

typedef struct DeclNode
{
    Node base;
    ObjString *name;
    Node *value;
    uint32_t flags;
} DeclNode;

// `if` node, used for both forms.
// Statement form: then_branch and else_branch are BLOCK nodes.
// Expression form: they are arbitrary expression nodes.
// eval_if returns whichever branch was executed.
typedef struct IfNode
{
    Node base;
    Node *cond;
    Node *then_branch;
    Node *else_branch;
} IfNode;

typedef struct WhileNode
{
    Node base;
    Node *cond;
    Node *body;
} WhileNode;

typedef struct ForRangeNode
{
    Node base;
    Node *init;
    Node *end;
    Node *step;
    Node *body;
} ForRangeNode;

typedef struct ForInNode
{
    Node base;
    Node *key;
    Node *value;
    Node *iterable;
    Node *body;
} ForInNode;

typedef struct BlockNode
{
    Node base;
    Node **stmts;
    int count;
} BlockNode;

typedef struct ReturnNode
{
    Node base;
    Node *value;
} ReturnNode;

typedef struct FunDeclNode
{
    Node base;
    ObjFunction *fn;
} FunDeclNode;

typedef struct TableNode
{
    Node base;
    Node **array_items;
    int array_count;
    ObjString **hash_keys;
    Node **hash_values;
    int hash_count;
} TableNode;

// Two parallel arrays: parts and, for expression parts only, an
// optional format spec. specs[i] is NULL when part i has no spec
// (literals never do). Both arrays have part_count entries.
typedef struct InterpNode
{
    Node base;
    Node **parts;
    ObjString **specs;
    int part_count;
} InterpNode;

// MultiDeclNode covers two shapes:
//   local a, b, c = expr    (flags & DECL_LOCAL, optionally DECL_CONST)
//   a, b, c = expr          (flags & DECL_ASSIGN)
typedef struct MultiDeclNode
{
    Node base;
    ObjString **names;
    int name_count;
    Node *value;
    uint32_t flags;
} MultiDeclNode;

Node *ast_alloc_node(size_t size, NodeType type,
                     int line, int col, bool is_expr);
Node **ast_copy_node_array(Node **src, int count);

Node *node_literal(int line, int col, Value value);
Node *node_var(int line, int col, ObjString *name);
Node *node_binary(int line, int col, Node *left, TokenType op, Node *right);
Node *node_unary(int line, int col, TokenType op, Node *operand);
Node *node_is(int line, int col, Node *left, ObjString *type_name);
Node *node_call(int line, int col, Node *callee, Node **args, int arg_count);
Node *node_index(int line, int col, Node *target, Node *index);
Node *node_member(int line, int col, Node *target, ObjString *name);
Node *node_assign(int line, int col, Node *target, TokenType op, Node *value);

Node *node_decl(int line, int col, ObjString *name, Node *value, uint32_t flags);
Node *node_if(int line, int col, Node *cond, Node *then_b, Node *else_b);
Node *node_while(int line, int col, Node *cond, Node *body);
Node *node_for_range(int line, int col, Node *init, Node *end, Node *step, Node *body);
Node *node_for_in(int line, int col, Node *key, Node *value, Node *iterable, Node *body);
Node *node_block(int line, int col, Node **stmts, int count);
Node *node_return(int line, int col, Node *value);
Node *node_break(int line, int col);
Node *node_continue(int line, int col);
Node *node_fun_decl(int line, int col, ObjFunction *fn);
Node *node_table(int line, int col,
                 Node **array_items, int array_count,
                 ObjString **hash_keys, Node **hash_values, int hash_count);
Node *node_interp(int line, int col, Node **parts, ObjString **specs, int count);
Node *node_multi_decl(int line, int col, ObjString **names, int name_count,
                      Node *value, uint32_t flags);

void node_free_tree(Node **n);

const char *node_type_name(NodeType t);

#endif // ART_AST_H
