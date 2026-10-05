// ============================================================
// pattern.c — Lua-style pattern matching engine
//
// Pure UTF-16 in, PatternMatch out. No Value, no GC, no
// interpreter knowledge beyond ArtState for error reporting.
// Any feature that needs text matching can call pattern_match.
//
// ============================================================

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pattern.h"
#include "interp.h"

#define L_ESC ((uint16_t)'%')
#define MAX_DEPTH 400

// Internal matcher state. Filled in by pattern_match, driven by
// the recursive match_here. Not exposed — callers see PatternMatch.
typedef struct
{
    ArtState *S;
    const uint16_t *src;
    int src_len;
    const uint16_t *pat;
    int pat_len;
    int level;
    int depth;
    int match_start; // 0-based offset in src
    struct
    {
        int init;
        int len; // -1 = unfinished, -2 = position capture
    } capture[PAT_MAX_CAPTURES];
} MatchState;

// Return pointer just past the pattern item starting at p.
// Handles escapes and sets (which can contain ']').
static const uint16_t *classend(MatchState *ms, const uint16_t *p)
{
    const uint16_t *end = ms->pat + ms->pat_len;

    switch (*p++)
    {
    case L_ESC:
        if (p == end)
            art_runtime_error(ms->S, NULL, "pattern ends with '%%'");
        return p + 1;

    case '[':
        if (p < end && *p == '^')
            p++;
        if (p < end && *p == ']')
            p++;
        while (p < end)
        {
            uint16_t c = *p++;
            if (c == L_ESC && p < end)
                p++;
            else if (c == ']')
                return p;
        }
        art_runtime_error(ms->S, NULL,
                          "malformed pattern (missing ']')");
        return NULL;

    default:
        return p;
    }
}

// Test one char against a character class (%a, %d, etc.).
// Uppercase classes test the complement.
static bool match_class(uint16_t c, uint16_t cl)
{
    bool res;
    uint16_t lower = cl;
    if (lower >= 'A' && lower <= 'Z')
        lower = (uint16_t)(lower - 'A' + 'a');

    int ci = (c <= 0xFF) ? (int)c : -1;

    switch (lower)
    {
    case 'a':
        res = (ci >= 0) && isalpha(ci) != 0;
        break;
    case 'c':
        res = (ci >= 0) && iscntrl(ci) != 0;
        break;
    case 'd':
        res = (ci >= 0) && isdigit(ci) != 0;
        break;
    case 'g':
        res = (ci >= 0) && isgraph(ci) != 0;
        break;
    case 'l':
        res = (ci >= 0) && islower(ci) != 0;
        break;
    case 'p':
        res = (ci >= 0) && ispunct(ci) != 0;
        break;
    case 's':
        res = (ci >= 0) && isspace(ci) != 0;
        break;
    case 'u':
        res = (ci >= 0) && isupper(ci) != 0;
        break;
    case 'w':
        res = (ci >= 0) && isalnum(ci) != 0;
        break;
    case 'x':
        res = (ci >= 0) && isxdigit(ci) != 0;
        break;
    default:
        return cl == c;
    }

    if (cl >= 'A' && cl <= 'Z')
        res = !res;
    return res;
}

// Test one char against a bracket set. p points at '[', ec at ']'.
static bool match_set(uint16_t c, const uint16_t *p, const uint16_t *ec)
{
    bool invert = false;
    p++; // skip '['
    if (p < ec && *p == '^')
    {
        invert = true;
        p++;
    }

    while (p < ec)
    {
        if (*p == L_ESC && p + 1 < ec)
        {
            p++;
            if (match_class(c, *p))
                return !invert;
            p++;
        }
        else if (p + 2 < ec && p[1] == '-')
        {
            uint16_t lo = *p;
            uint16_t hi = p[2];
            if (lo <= c && c <= hi)
                return !invert;
            p += 3;
        }
        else
        {
            if (*p == c)
                return !invert;
            p++;
        }
    }
    return invert;
}

// Test one char against the pattern item at p..ep.
static bool single_match(MatchState *ms, const uint16_t *s,
                         const uint16_t *p, const uint16_t *ep)
{
    if (s >= ms->src + ms->src_len)
        return false;
    uint16_t c = *s;

    switch (*p)
    {
    case '.':
        return true;
    case L_ESC:
        return match_class(c, p[1]);
    case '[':
        return match_set(c, p, ep - 1);
    default:
        return *p == c;
    }
}

static const uint16_t *match_here(MatchState *ms, const uint16_t *s,
                                  const uint16_t *p);

// Greedy quantifier: consume as many as possible, then backtrack.
static const uint16_t *max_expand(MatchState *ms, const uint16_t *s,
                                  const uint16_t *p, const uint16_t *ep)
{
    int i = 0;
    while (single_match(ms, s + i, p, ep))
        i++;

    while (i >= 0)
    {
        const uint16_t *r = match_here(ms, s + i, ep + 1);
        if (r != NULL)
            return r;
        i--;
    }
    return NULL;
}

// Lazy quantifier: consume as few as possible, then expand.
static const uint16_t *min_expand(MatchState *ms, const uint16_t *s,
                                  const uint16_t *p, const uint16_t *ep)
{
    for (;;)
    {
        const uint16_t *r = match_here(ms, s, ep + 1);
        if (r != NULL)
            return r;
        if (single_match(ms, s, p, ep))
            s++;
        else
            return NULL;
    }
}

static const uint16_t *match_here(MatchState *ms, const uint16_t *s,
                                  const uint16_t *p)
{
    if (++ms->depth > MAX_DEPTH)
        art_runtime_error(ms->S, NULL, "pattern nested too deeply");

    const uint16_t *pat_end = ms->pat + ms->pat_len;
    const uint16_t *src_end = ms->src + ms->src_len;
    const uint16_t *result = NULL;

    for (;;)
    {
        if (p >= pat_end)
        {
            result = s;
            break;
        }

        uint16_t c = *p;

        // ----- start capture -----
        if (c == '(')
        {
            if (ms->level >= PAT_MAX_CAPTURES)
                art_runtime_error(ms->S, NULL, "too many captures");

            int lvl = ms->level++;
            ms->capture[lvl].init = (int)(s - ms->src);

            if (p + 1 < pat_end && p[1] == ')')
            {
                ms->capture[lvl].len = -2;
                result = match_here(ms, s, p + 2);
            }
            else
            {
                ms->capture[lvl].len = -1;
                result = match_here(ms, s, p + 1);
            }

            if (result == NULL)
                ms->level--;
            break;
        }

        // ----- end capture -----
        if (c == ')')
        {
            if (ms->level == 0)
                art_runtime_error(ms->S, NULL,
                                  "unmatched ')' in pattern");

            int lvl = ms->level - 1;
            while (lvl >= 0 && ms->capture[lvl].len != -1)
                lvl--;
            if (lvl < 0)
                art_runtime_error(ms->S, NULL,
                                  "unmatched ')' in pattern");

            int saved = ms->capture[lvl].len;
            ms->capture[lvl].len =
                (int)(s - ms->src) - ms->capture[lvl].init;

            result = match_here(ms, s, p + 1);
            if (result == NULL)
                ms->capture[lvl].len = saved;
            break;
        }

        // ----- end anchor -----
        if (c == '$' && p + 1 == pat_end)
        {
            result = (s == src_end) ? s : NULL;
            break;
        }

        // ----- escape sequences -----
        if (c == L_ESC && p + 1 < pat_end)
        {
            uint16_t next = p[1];

            // Balanced match: %bxy
            if (next == 'b')
            {
                if (p + 3 >= pat_end)
                    art_runtime_error(ms->S, NULL,
                                      "malformed pattern "
                                      "(missing arguments to '%%b')");
                uint16_t open = p[2];
                uint16_t close = p[3];

                if (s >= src_end || *s != open)
                {
                    result = NULL;
                    break;
                }

                int bdepth = 1;
                const uint16_t *q = s + 1;
                while (q < src_end && bdepth > 0)
                {
                    if (*q == close)
                        bdepth--;
                    else if (*q == open)
                        bdepth++;
                    q++;
                }
                if (bdepth != 0)
                {
                    result = NULL;
                    break;
                }

                s = q;
                p += 4;
                continue;
            }

            // Frontier: %f[set]
            if (next == 'f')
            {
                p += 2;
                if (p >= pat_end || *p != '[')
                    art_runtime_error(ms->S, NULL,
                                      "missing '[' after '%%f' in pattern");

                const uint16_t *ep = classend(ms, p);
                uint16_t prev = (s == ms->src) ? 0 : s[-1];
                uint16_t cur = (s < src_end) ? *s : 0;

                if (!match_set(prev, p, ep - 1) &&
                    match_set(cur, p, ep - 1))
                {
                    p = ep;
                    continue;
                }
                result = NULL;
                break;
            }

            // Back-reference: %1..%9
            if (next >= '1' && next <= '9')
            {
                int idx = next - '1';
                if (idx >= ms->level || ms->capture[idx].len < 0)
                    art_runtime_error(ms->S, NULL,
                                      "invalid capture index %%%c", next);

                int len = ms->capture[idx].len;
                const uint16_t *cap = ms->src + ms->capture[idx].init;
                if (s + len > src_end ||
                    memcmp(s, cap,
                           (size_t)len * sizeof(uint16_t)) != 0)
                {
                    result = NULL;
                    break;
                }
                s += len;
                p += 2;
                continue;
            }
            // Otherwise fall through to single-char matching.
        }

        // ----- single pattern item, possibly quantified -----
        const uint16_t *ep = classend(ms, p);
        bool m = single_match(ms, s, p, ep);

        if (ep < pat_end)
        {
            uint16_t q = *ep;

            if (q == '?')
            {
                if (m)
                {
                    result = match_here(ms, s + 1, ep + 1);
                    if (result != NULL)
                        break;
                }
                p = ep + 1;
                continue;
            }
            if (q == '*')
            {
                result = max_expand(ms, s, p, ep);
                break;
            }
            if (q == '+')
            {
                result = m ? max_expand(ms, s + 1, p, ep)
                           : NULL;
                break;
            }
            if (q == '-')
            {
                result = min_expand(ms, s, p, ep);
                break;
            }
        }

        if (!m)
        {
            result = NULL;
            break;
        }
        s++;
        p = ep;
    }

    ms->depth--;
    return result;
}

// Try to match at `start` (or anywhere from there if no ^).
// Returns pointer past the match, or NULL. Sets ms->match_start.
static const uint16_t *do_match_at(MatchState *ms, const uint16_t *s,
                                   const uint16_t *p)
{
    const uint16_t *src_end = ms->src + ms->src_len;

    if (p < ms->pat + ms->pat_len && *p == '^')
    {
        ms->level = 0;
        ms->depth = 0;
        ms->match_start = (int)(s - ms->src);
        const uint16_t *r = match_here(ms, s, p + 1);
        return r;
    }

    const uint16_t *pos = s;
    for (;;)
    {
        ms->level = 0;
        ms->depth = 0;
        const uint16_t *r = match_here(ms, pos, p);
        if (r != NULL)
        {
            ms->match_start = (int)(pos - ms->src);
            return r;
        }
        if (pos >= src_end)
            return NULL;
        pos++;
    }
}

// ============================================================
// Public entry point
// ============================================================

bool pattern_match(ArtState *S,
                   const uint16_t *src, int src_len,
                   const uint16_t *pat, int pat_len,
                   int init,
                   PatternMatch *out)
{
    if (init < 0)
        init = 0;
    if (init > src_len)
        init = src_len;

    MatchState ms;
    ms.S = S;
    ms.src = src;
    ms.src_len = src_len;
    ms.pat = pat;
    ms.pat_len = pat_len;

    const uint16_t *found = do_match_at(&ms,
                                        src + init,
                                        pat);
    if (found == NULL)
        return false;

    out->match_start = ms.match_start;
    out->match_len = (int)(found - (src + ms.match_start));

    int ncaps = 0;
    while (ncaps < ms.level && ms.capture[ncaps].len != -1)
        ncaps++;

    out->ncaptures = ncaps;
    for (int i = 0; i < ncaps; i++)
    {
        out->caps[i].start = ms.capture[i].init;
        out->caps[i].len = ms.capture[i].len;
    }

    return true;
}