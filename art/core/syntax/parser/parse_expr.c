// ============================================================
// parse_expr.c — Pratt parser for expressions
//
// Also home to parse_if_any, the unified `if` parser. `if`
// appears both as a statement (branches are blocks) and as an
// expression (branches are values):
//
//     if (cond) { a } else { b }         // statement
//     local x = if (cond) -> a else b    // expression
//     local x = if (cond) -> a           // expression, no else
//
// The expression form allows omitting `else`. When the
// condition is false and there is no else, the expression
// evaluates to nil. This matches the statement form, where
// `if (c) { ... }` without an else simply does nothing when c
// is false.
//
// Both forms produce an IfNode. eval_if returns the value of
// whichever branch ran, or nil when the false branch is absent.
// ============================================================

#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "features/features.h"

typedef enum
{
    PREC_NONE = 0,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARE,
    PREC_TERM,
    PREC_FACTOR,
    PREC_POWER,
    PREC_UNARY,
    PREC_CALL,
} Precedence;

static int get_lb(TokenType t)
{
    switch (t)
    {
    case TOKEN_OR:
        return PREC_OR;
    case TOKEN_AND:
        return PREC_AND;
    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
        return PREC_EQUALITY;
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_IS:
    case TOKEN_IN:
        return PREC_COMPARE;
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        return PREC_TERM;
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        return PREC_FACTOR;
    case TOKEN_CARET:
        return PREC_POWER;
    case TOKEN_LEFT_PAREN:
    case TOKEN_LEFT_BRACKET:
    case TOKEN_DOT:
        return PREC_CALL;
    default:
        return PREC_NONE;
    }
}

static bool is_right_assoc(TokenType t)
{
    return t == TOKEN_CARET;
}

static bool is_assign_op(TokenType t)
{
    switch (t)
    {
    case TOKEN_EQUAL:
    case TOKEN_PLUS_EQUAL:
    case TOKEN_MINUS_EQUAL:
    case TOKEN_STAR_EQUAL:
    case TOKEN_SLASH_EQUAL:
    case TOKEN_PERCENT_EQUAL:
    case TOKEN_CARET_EQUAL:
        return true;
    default:
        return false;
    }
}

static bool is_assignable(Node *n)
{
    return n->type == NODE_VAR ||
           n->type == NODE_MEMBER ||
           n->type == NODE_INDEX;
}

static bool token_is_word_like(const Token *t)
{
    if (t->type == TOKEN_ERROR || t->type == TOKEN_EOF)
        return false;
    if (t->length <= 0 || t->start == NULL)
        return false;
    char c = t->start[0];
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static Node *parse_precedence(Parser *P, int min_lb);
static Node *parse_prefix(Parser *P, TokenType type);
static Node *parse_group_or_lambda(Parser *P);
static Node *parse_lambda_body(Parser *P, Token start_tok,
                               ObjString **params, int param_count);
static Node *parse_fun_expr(Parser *P);
static Node *parse_table_literal(Parser *P);
static Node *parse_call_args(Parser *P, Node *callee, Token open_paren);
static Node *parse_index(Parser *P, Node *target, Token open_bracket);
static Node *parse_member(Parser *P, Node *target, Token dot);

static Node *parse_assignment(Parser *P);

Node *parse_expression(Parser *P)
{
    return parse_assignment(P);
}

static Node *parse_assignment(Parser *P)
{
    Node *left = parse_precedence(P, PREC_NONE);
    if (left == NULL)
        return NULL;

    if (is_assign_op(P->current.type))
    {
        Token op = P->current;

        if (!is_assignable(left))
        {
            parser_error(P, "invalid assignment target");
            node_free_tree(&left);
            return NULL;
        }

        parser_advance(P);
        Node *value = parse_assignment(P);
        if (value == NULL)
        {
            node_free_tree(&left);
            return NULL;
        }
        return node_assign(op.line, op.column, left, op.type, value);
    }

    return left;
}

// ============================================================
// if — statement form and expression form
//
// Called from two places with identical parser state (the `if`
// token is in P->previous; P->current is the `(` that follows):
//   - the statement dispatch table, for `if` at statement level
//   - parse_prefix, for `if` in expression position
//
// The two forms differ only in what follows the closing `)`:
// `->` means expression form (branches are values), anything
// else means statement form (branches are blocks).
//
// The expression form's `else` is optional. `if (c) -> a`
// evaluates to nil when c is false.
// ============================================================

Node *parse_if_any(Parser *P)
{
    Token if_tok = P->previous;
    parser_consume(P, TOKEN_LEFT_PAREN, "expected '(' after 'if'");
    Node *cond = parse_expression(P);
    if (cond == NULL)
        return NULL;
    parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after condition");

    // Expression form: `if (c) -> a` or `if (c) -> a else b`.
    if (parser_match(P, TOKEN_ARROW))
    {
        Node *then_b = parse_expression(P);
        if (then_b == NULL)
        {
            node_free_tree(&cond);
            return NULL;
        }

        Node *else_b = NULL;
        if (parser_match(P, TOKEN_ELSE))
        {
            else_b = parse_expression(P);
            if (else_b == NULL)
            {
                node_free_tree(&cond);
                node_free_tree(&then_b);
                return NULL;
            }
        }

        return node_if(if_tok.line, if_tok.column, cond, then_b, else_b);
    }

    // Statement form. `else if` chains recurse into this same
    // function so nesting works either way.
    Node *then_b = parse_block(P);
    if (then_b == NULL)
    {
        node_free_tree(&cond);
        return NULL;
    }

    Node *else_b = NULL;
    if (parser_match(P, TOKEN_ELSE))
    {
        if (parser_check(P, TOKEN_IF))
        {
            parser_advance(P);
            else_b = parse_if_any(P);
        }
        else
        {
            else_b = parse_block(P);
        }
    }

    return node_if(if_tok.line, if_tok.column, cond, then_b, else_b);
}

static bool string_has_interp(Token *t)
{
    const char *src = t->start + 1;
    int len = t->length - 2;

    for (int i = 0; i < len; i++)
    {
        if (src[i] == '\\')
        {
            i++;
            continue;
        }
        if (src[i] == '$' && i + 1 < len && src[i + 1] == '{')
            return true;
    }
    return false;
}

static Node *parse_substring_expression(Parser *P, const char *src,
                                        int len, Token at)
{
    Parser sub = *P;
    lexer_init(&sub.lexer, P->state, src, len);
    sub.current = lexer_next(&sub.lexer);
    sub.previous = sub.current;
    sub.had_error = false;
    sub.error_count = 0;
    sub.panic_mode = false;

    Node *expr = parse_expression(&sub);

    if (sub.had_error)
    {
        P->had_error = true;
        P->error_count += sub.error_count;
    }
    if (sub.current.type != TOKEN_EOF)
    {
        parser_error_at(P, &at, "unexpected tokens in interpolation");
        if (expr)
            node_free_tree(&expr);
        return NULL;
    }
    return expr;
}

static char decode_escape(Parser *P, Token *t, char e)
{
    switch (e)
    {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case '\\':
        return '\\';
    case '"':
        return '"';
    case '\'':
        return '\'';
    case '$':
        return '$';
    default:
        parser_error_at(P, t, "unknown escape sequence");
        return e;
    }
}

static Node *parse_interp_string(Parser *P, Token t)
{
    const char *src = t.start + 1;
    int len = t.length - 2;

    int cap = 8, count = 0;
    Node **parts = malloc(sizeof(Node *) * cap);

    char *buf = malloc(len + 1);
    int buf_len = 0;

    int i = 0;
    while (i < len)
    {
        char c = src[i];

        if (c == '\\' && i + 1 < len)
        {
            char e = src[i + 1];
            i += 2;
            buf[buf_len++] = decode_escape(P, &t, e);
            continue;
        }

        if (c == '$' && i + 1 < len && src[i + 1] == '{')
        {
            if (buf_len > 0)
            {
                ObjString *seg = obj_string_from_utf8(P->state, buf, buf_len);
                if (count >= cap)
                {
                    cap *= 2;
                    parts = realloc(parts, sizeof(Node *) * cap);
                }
                parts[count++] = node_literal(t.line, t.column, OBJ_VAL(seg));
                buf_len = 0;
            }

            int expr_start = i + 2;
            int j = expr_start;
            int depth = 1;
            while (j < len && depth > 0)
            {
                if (src[j] == '\\')
                {
                    j += 2;
                    continue;
                }
                if (src[j] == '{')
                    depth++;
                else if (src[j] == '}')
                    depth--;
                if (depth == 0)
                    break;
                j++;
            }
            if (depth != 0)
            {
                parser_error_at(P, &t, "unterminated interpolation");
                for (int k = 0; k < count; k++)
                    node_free_tree(&parts[k]);
                free(parts);
                free(buf);
                return NULL;
            }

            Node *expr = parse_substring_expression(P, src + expr_start,
                                                    j - expr_start, t);
            if (expr == NULL)
            {
                for (int k = 0; k < count; k++)
                    node_free_tree(&parts[k]);
                free(parts);
                free(buf);
                return NULL;
            }

            if (count >= cap)
            {
                cap *= 2;
                parts = realloc(parts, sizeof(Node *) * cap);
            }
            parts[count++] = expr;
            i = j + 1;
            continue;
        }

        buf[buf_len++] = c;
        i++;
    }

    if (buf_len > 0)
    {
        ObjString *seg = obj_string_from_utf8(P->state, buf, buf_len);
        if (count >= cap)
        {
            cap *= 2;
            parts = realloc(parts, sizeof(Node *) * cap);
        }
        parts[count++] = node_literal(t.line, t.column, OBJ_VAL(seg));
    }

    free(buf);

    Node *result = node_interp(t.line, t.column, parts, count);
    free(parts);
    return result;
}

static Node *parse_precedence(Parser *P, int min_lb)
{
    parser_advance(P);
    Node *left = parse_prefix(P, P->previous.type);
    if (left == NULL)
        return NULL;

    for (;;)
    {
        if (P->paren_depth == 0 && P->current.line > P->previous.line && !token_continues_expression(P->current.type))
            break;

        int lb = get_lb(P->current.type);
        if (lb <= min_lb)
            break;

        Token op_tok = P->current;

        if (op_tok.type == TOKEN_LEFT_PAREN)
        {
            parser_advance(P);
            left = parse_call_args(P, left, op_tok);
        }
        else if (op_tok.type == TOKEN_LEFT_BRACKET)
        {
            parser_advance(P);
            left = parse_index(P, left, op_tok);
        }
        else if (op_tok.type == TOKEN_DOT)
        {
            parser_advance(P);
            left = parse_member(P, left, op_tok);
        }
        else if (op_tok.type == TOKEN_IS)
        {
            parser_advance(P);
            Token tn = parser_consume(P, TOKEN_IDENT,
                                      "expected type name after 'is'");
            ObjString *name = parser_token_to_string(P, &tn);
            left = node_is(op_tok.line, op_tok.column, left, name);
        }
        else
        {
            parser_advance(P);
            int right_lb = lb - (is_right_assoc(op_tok.type) ? 1 : 0);
            Node *right = parse_precedence(P, right_lb);
            if (right == NULL)
            {
                node_free_tree(&left);
                return NULL;
            }
            left = node_binary(op_tok.line, op_tok.column,
                               left, op_tok.type, right);
        }

        if (left == NULL)
            return NULL;
    }

    return left;
}

static Node *parse_prefix(Parser *P, TokenType type)
{
    switch (type)
    {
    case TOKEN_INT:
    case TOKEN_FLOAT:
        return node_literal(P->previous.line, P->previous.column,
                            P->previous.literal);

    case TOKEN_STRING:
    {
        Token t = P->previous;
        if (!string_has_interp(&t))
            return node_literal(t.line, t.column, t.literal);
        return parse_interp_string(P, t);
    }

    case TOKEN_TRUE:
        return node_literal(P->previous.line, P->previous.column,
                            BOOL_VAL(true));
    case TOKEN_FALSE:
        return node_literal(P->previous.line, P->previous.column,
                            BOOL_VAL(false));
    case TOKEN_NIL:
        return node_literal(P->previous.line, P->previous.column,
                            NIL_VAL);

    case TOKEN_IF:
        return parse_if_any(P);

    case TOKEN_IDENT:
    {
        Token ident_tok = P->previous;

        if (!P->in_switch_case && parser_check(P, TOKEN_ARROW))
        {
            Token ident = P->previous;
            parser_advance(P);
            ObjString *param = parser_token_to_string(P, &ident);
            return parse_lambda_body(P, ident, &param, 1);
        }

        ObjString *name = parser_token_to_string(P, &ident_tok);
        Node *var_node = node_var(ident_tok.line, ident_tok.column, name);

        if (parser_check(P, TOKEN_STRING))
        {
            Token arg_tok = P->current;
            parser_advance(P);
            Node *arg = node_literal(arg_tok.line, arg_tok.column,
                                     arg_tok.literal);
            Node *args[1] = {arg};
            return node_call(ident_tok.line, ident_tok.column,
                             var_node, args, 1);
        }

        return var_node;
    }

    case TOKEN_LEFT_PAREN:
        return parse_group_or_lambda(P);

    case TOKEN_LEFT_BRACKET:
        return parse_table_literal(P);

    case TOKEN_FUN:
        return parse_fun_expr(P);

    case TOKEN_MINUS:
    case TOKEN_BANG:
    {
        Token op = P->previous;
        Node *operand = parse_precedence(P, PREC_UNARY);
        if (operand == NULL)
            return NULL;
        return node_unary(op.line, op.column, op.type, operand);
    }

    case TOKEN_THIS:
        return node_this(P->previous.line, P->previous.column);
    case TOKEN_SUPER:
        return node_super(P->previous.line, P->previous.column);

    case TOKEN_SWITCH:
        return switch_parse(P);

    default:
        parser_error(P, "expected expression, got %s",
                     token_type_name(type));
        return NULL;
    }
}

static Node *parse_group_or_lambda(Parser *P)
{
    Token open_paren = P->previous;

    if (parser_match(P, TOKEN_RIGHT_PAREN))
    {
        if (!parser_match(P, TOKEN_ARROW))
        {
            parser_error(P, "expected '->' after '()'");
            return NULL;
        }
        return parse_lambda_body(P, open_paren, NULL, 0);
    }

    Node *first = parse_expression(P);
    if (first == NULL)
        return NULL;

    if (parser_match(P, TOKEN_COMMA))
    {
        int cap = 4, count = 1;
        ObjString **params = malloc(sizeof(ObjString *) * cap);
        if (first->type != NODE_VAR)
        {
            parser_error(P, "lambda parameter must be an identifier");
            free(params);
            node_free_tree(&first);
            return NULL;
        }
        params[0] = ((VarNode *)first)->name;
        node_free_tree(&first);

        for (;;)
        {
            if (!parser_check(P, TOKEN_IDENT))
            {
                parser_error(P, "expected identifier in lambda parameter list");
                free(params);
                return NULL;
            }
            Token p = P->current;
            parser_advance(P);
            if (count >= cap)
            {
                cap *= 2;
                params = realloc(params, sizeof(ObjString *) * cap);
            }
            params[count++] = parser_token_to_string(P, &p);

            if (!parser_match(P, TOKEN_COMMA))
                break;
        }

        parser_consume(P, TOKEN_RIGHT_PAREN,
                       "expected ')' after lambda parameters");
        if (!parser_match(P, TOKEN_ARROW))
        {
            parser_error(P, "expected '->' after parameter list");
            free(params);
            return NULL;
        }

        Node *result = parse_lambda_body(P, open_paren, params, count);
        free(params);
        return result;
    }

    parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after expression");

    if (parser_match(P, TOKEN_ARROW))
    {
        if (first->type != NODE_VAR)
        {
            parser_error(P, "lambda parameter must be an identifier");
            node_free_tree(&first);
            return NULL;
        }
        ObjString *param = ((VarNode *)first)->name;
        node_free_tree(&first);
        return parse_lambda_body(P, open_paren, &param, 1);
    }

    return first;
}

static Node *parse_lambda_body(Parser *P, Token start_tok,
                               ObjString **params, int param_count)
{
    ObjFunction *fn = parse_function_body(P, NULL, params, param_count, false);
    if (fn == NULL)
        return NULL;
    return node_fun_decl(start_tok.line, start_tok.column, fn);
}

static Node *parse_fun_expr(Parser *P)
{
    Token fun_tok = P->previous;
    ObjString *name = NULL;

    if (parser_check(P, TOKEN_IDENT))
    {
        Token name_tok = P->current;
        parser_advance(P);
        name = parser_token_to_string(P, &name_tok);
    }

    parser_consume(P, TOKEN_LEFT_PAREN, "expected '(' after 'fun'");

    int cap = 4, count = 0;
    ObjString **pnames = NULL;
    ObjString **ptypes = NULL;
    Node **pdefs = NULL;
    bool variadic = false;

    if (!parser_check(P, TOKEN_RIGHT_PAREN))
    {
        pnames = malloc(sizeof(ObjString *) * cap);
        ptypes = malloc(sizeof(ObjString *) * cap);
        pdefs = malloc(sizeof(Node *) * cap);

        bool seen_default = false;

        for (;;)
        {
            if (count >= cap)
            {
                cap *= 2;
                pnames = realloc(pnames, sizeof(ObjString *) * cap);
                ptypes = realloc(ptypes, sizeof(ObjString *) * cap);
                pdefs = realloc(pdefs, sizeof(Node *) * cap);
            }

            if (parser_match(P, TOKEN_DOT_DOT_DOT))
            {
                Token rn = parser_consume(P, TOKEN_IDENT,
                                          "expected name after '...'");
                pnames[count] = parser_token_to_string(P, &rn);
                ptypes[count] = NULL;
                pdefs[count] = NULL;
                count++;
                variadic = true;
                break;
            }

            Token pn = parser_consume(P, TOKEN_IDENT, "expected parameter name");
            pnames[count] = parser_token_to_string(P, &pn);
            ptypes[count] = NULL;
            pdefs[count] = NULL;

            if (parser_match(P, TOKEN_COLON))
            {
                Token tn = parser_consume(P, TOKEN_IDENT,
                                          "expected type name after ':'");
                ptypes[count] = parser_token_to_string(P, &tn);
            }

            if (parser_match(P, TOKEN_EQUAL))
            {
                pdefs[count] = parse_expression(P);
                seen_default = true;
            }
            else if (seen_default)
            {
                parser_error(P,
                             "parameter without default after a default parameter");
            }

            count++;

            if (!parser_match(P, TOKEN_COMMA))
                break;
            if (parser_check(P, TOKEN_RIGHT_PAREN))
                break;
        }
    }

    parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after parameters");

    ObjFunction *fn = parse_function_body(P, name, pnames, count, variadic);
    if (fn != NULL && count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            fn->params[i].type_name = ptypes[i];
            fn->params[i].default_value = pdefs[i];
        }
    }
    free(pnames);
    free(ptypes);
    free(pdefs);

    if (fn == NULL)
        return NULL;
    return node_fun_decl(fun_tok.line, fun_tok.column, fn);
}

static Node *parse_table_literal(Parser *P)
{
    Token open = P->previous;

    int arr_cap = 4, arr_count = 0;
    Node **arr_items = malloc(sizeof(Node *) * arr_cap);

    int hash_cap = 4, hash_count = 0;
    ObjString **hash_keys = malloc(sizeof(ObjString *) * hash_cap);
    Node **hash_values = malloc(sizeof(Node *) * hash_cap);

    if (!parser_check(P, TOKEN_RIGHT_BRACKET))
    {
        do
        {
            bool is_hash_pair = false;
            Token saved_current = P->current;
            Token saved_previous = P->previous;
            Lexer saved_lexer = P->lexer;

            if (parser_check(P, TOKEN_STRING))
            {
                parser_advance(P);
                if (parser_check(P, TOKEN_EQUAL))
                {
                    is_hash_pair = true;
                }
                else
                {
                    P->current = saved_current;
                    P->previous = saved_previous;
                    P->lexer = saved_lexer;
                }
            }

            if (is_hash_pair)
            {
                parser_advance(P);
                Node *val = parse_expression(P);
                if (val == NULL)
                    goto fail;

                if (hash_count >= hash_cap)
                {
                    hash_cap *= 2;
                    hash_keys = realloc(hash_keys,
                                        sizeof(ObjString *) * hash_cap);
                    hash_values = realloc(hash_values,
                                          sizeof(Node *) * hash_cap);
                }
                hash_keys[hash_count] = AS_STRING(saved_current.literal);
                hash_values[hash_count++] = val;
            }
            else
            {
                Node *item = parse_expression(P);
                if (item == NULL)
                    goto fail;

                if (arr_count >= arr_cap)
                {
                    arr_cap *= 2;
                    arr_items = realloc(arr_items, sizeof(Node *) * arr_cap);
                }
                arr_items[arr_count++] = item;
            }
        } while (parser_match(P, TOKEN_COMMA));
    }

    parser_consume(P, TOKEN_RIGHT_BRACKET,
                   "expected ']' to close table literal");

    {
        Node *result = node_table(open.line, open.column,
                                  arr_items, arr_count,
                                  hash_keys, hash_values, hash_count);
        free(arr_items);
        free(hash_keys);
        free(hash_values);
        return result;
    }

fail:
    for (int i = 0; i < arr_count; i++)
        node_free_tree(&arr_items[i]);
    free(arr_items);
    for (int i = 0; i < hash_count; i++)
        node_free_tree(&hash_values[i]);
    free(hash_keys);
    free(hash_values);
    return NULL;
}

static Node *parse_call_args(Parser *P, Node *callee, Token open_paren)
{
    int cap = 4, count = 0;
    Node **args = NULL;

    if (!parser_check(P, TOKEN_RIGHT_PAREN))
    {
        args = malloc(sizeof(Node *) * cap);
        do
        {
            if (count >= cap)
            {
                cap *= 2;
                args = realloc(args, sizeof(Node *) * cap);
            }
            Node *arg = parse_expression(P);
            if (arg == NULL)
            {
                for (int i = 0; i < count; i++)
                    node_free_tree(&args[i]);
                free(args);
                return NULL;
            }
            args[count++] = arg;
        } while (parser_match(P, TOKEN_COMMA));
    }

    parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after arguments");

    Node *result = node_call(open_paren.line, open_paren.column,
                             callee, args, count);
    free(args);
    return result;
}

static Node *parse_index(Parser *P, Node *target, Token open_bracket)
{
    Node *index = parse_expression(P);
    if (index == NULL)
        return NULL;
    parser_consume(P, TOKEN_RIGHT_BRACKET, "expected ']' after index");
    return node_index(open_bracket.line, open_bracket.column, target, index);
}

static Node *parse_member(Parser *P, Node *target, Token dot)
{
    if (!token_is_word_like(&P->current))
    {
        parser_error(P, "expected member name after '.'");
        return target;
    }

    Token name_tok = P->current;
    parser_advance(P);
    ObjString *name = parser_token_to_string(P, &name_tok);
    return node_member(dot.line, dot.column, target, name);
}

ObjFunction *parse_function_body(Parser *P, ObjString *name,
                                 ObjString **params, int param_count,
                                 bool variadic)
{
    ObjFunction *fn = obj_function_new(P->state, name);
    GC_PUSH(P->state, OBJ_VAL(fn));

    fn->is_variadic = variadic;
    fn->arity = variadic ? param_count - 1 : param_count;

    if (param_count > 0)
    {
        fn->params = malloc(sizeof(Param) * param_count);
        for (int i = 0; i < param_count; i++)
        {
            fn->params[i].name = params[i];
            fn->params[i].type_name = NULL;
            fn->params[i].type_class = NULL;
            fn->params[i].type_interface = NULL;
            fn->params[i].default_value = NULL;
        }
    }

    Node *body;
    if (parser_check(P, TOKEN_LEFT_BRACE))
    {
        body = parse_block(P);
    }
    else
    {
        Node *expr = parse_expression(P);
        if (expr == NULL)
        {
            GC_POP(P->state, 1);
            return NULL;
        }
        Node *ret = node_return(expr->line, expr->column, expr);
        body = node_block(expr->line, expr->column, &ret, 1);
    }

    if (body == NULL)
    {
        GC_POP(P->state, 1);
        return NULL;
    }

    fn->body = body;
    GC_POP(P->state, 1);
    return fn;
}
