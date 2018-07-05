#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <math.h>
#include <errno.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1] != 0; ++i)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;

    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    /* char* end; */
    const char* p = c->json;
    int state = 1; /* DFA */
    /* validate number by DFA */
    while (1)
    {
        if (*p == '\0' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            break;

        switch (state) {
            case 1: { /* start */
                if ('-' == *p)
                    state = 2;
                else if ('0' == *p)
                    state = 3;
                else if (ISDIGIT1TO9(*p))
                    state = 4;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            case 2: { /* negative */
                if ('0' == *p)
                    state = 3;
                else if (ISDIGIT1TO9(*p))
                    state = 4;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            case 3: { /* single 0 */
                if ('.' == *p)
                    state = 5;
                else if ('e' == *p || 'E' == *p)
                    state = 7;
                else
                    return LEPT_PARSE_ROOT_NOT_SINGULAR;
                break;
            }
            case 4: { /* integer */
                if ('.' == *p)
                    state = 5;
                else if ('e' == *p || 'E' == *p)
                    state = 7;
                else if (ISDIGIT(*p))
                    state = 4;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            case 5: { /* fraction */
                if (ISDIGIT(*p))
                    state = 6;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            case 6: { /* fraction end */
                if ('e' == *p || 'E' == *p)
                    state = 7;
                else if (ISDIGIT(*p))
                    state = 6;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            case 7: { /* exp */
                if ('-' == *p || '+' == *p)
                    state = 8;
                else if (ISDIGIT(*p))
                    state = 9;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            case 8: { /* exp number */
                if (ISDIGIT(*p))
                    state = 9;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            case 9: { /* exp end */
                if (ISDIGIT(*p))
                    state = 9;
                else
                    return LEPT_PARSE_INVALID_VALUE;
                break;
            }
            default:
                return LEPT_PARSE_INVALID_VALUE;
        }
        ++p;
    }

    if (1 == state || 2 == state || 5 == state || 7 == state || 8 == state)
        return LEPT_PARSE_INVALID_VALUE;

    v->n = strtod(c->json, NULL/*&end*/);
    /*if (c->json == end)
        return LEPT_PARSE_INVALID_VALUE;*/
    if (errno == ERANGE && (HUGE_VAL == v->n || -HUGE_VAL == v->n)) /* ERANGE == errno */
        return LEPT_PARSE_NUMBER_TOO_BIG;

    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return lept_parse_number(c, v);
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
