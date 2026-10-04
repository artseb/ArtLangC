// ============================================================
// number_feature.c — arithmetic and comparison for numbers
// ============================================================

#include <math.h>

#include "number.h"
#include "feature.h"
#include "interp.h"
#include "token.h"

static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        q--;
    return q;
}

static int64_t floor_mod(int64_t a, int64_t b)
{
    int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        r += b;
    return r;
}

static bool number_binop(ArtState *S, Node *at, TokenType op,
                         Value a, Value b, Value *out)
{
    if (op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
        op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL)
    {
        if (!IS_NUMBER(a) || !IS_NUMBER(b))
            return false;

        double x = AS_NUMBER(a);
        double y = AS_NUMBER(b);

        switch (op)
        {
        case TOKEN_LESS:          *out = BOOL_VAL(x < y);  return true;
        case TOKEN_LESS_EQUAL:    *out = BOOL_VAL(x <= y); return true;
        case TOKEN_GREATER:       *out = BOOL_VAL(x > y);  return true;
        case TOKEN_GREATER_EQUAL: *out = BOOL_VAL(x >= y); return true;
        default:                  return false;
        }
    }

    if (op != TOKEN_PLUS && op != TOKEN_MINUS && op != TOKEN_STAR &&
        op != TOKEN_SLASH && op != TOKEN_PERCENT)
        return false;

    if (!IS_NUMBER(a) || !IS_NUMBER(b))
        return false;

    if (IS_INT(a) && IS_INT(b))
    {
        int64_t x = AS_INT(a);
        int64_t y = AS_INT(b);
        switch (op)
        {
        case TOKEN_PLUS:    *out = INT_VAL(x + y);          return true;
        case TOKEN_MINUS:   *out = INT_VAL(x - y);          return true;
        case TOKEN_STAR:    *out = INT_VAL(x * y);          return true;
        case TOKEN_SLASH:
            if (y == 0)
                art_runtime_error(S, at, "division by zero");
            *out = INT_VAL(floor_div(x, y));
            return true;
        case TOKEN_PERCENT:
            if (y == 0)
                art_runtime_error(S, at, "modulo by zero");
            *out = INT_VAL(floor_mod(x, y));
            return true;
        default:
            return false;
        }
    }

    double x = AS_NUMBER(a);
    double y = AS_NUMBER(b);
    switch (op)
    {
    case TOKEN_PLUS:    *out = FLOAT_VAL(x + y); return true;
    case TOKEN_MINUS:   *out = FLOAT_VAL(x - y); return true;
    case TOKEN_STAR:    *out = FLOAT_VAL(x * y); return true;
    case TOKEN_SLASH:
        if (y == 0.0)
            art_runtime_error(S, at, "division by zero");
        *out = FLOAT_VAL(x / y);
        return true;
    case TOKEN_PERCENT:
        if (y == 0.0)
            art_runtime_error(S, at, "modulo by zero");
        *out = FLOAT_VAL(fmod(x, y));
        return true;
    default:
        return false;
    }
}

Feature number_feature = {
    .name = "number",
    .binop = number_binop,
};
