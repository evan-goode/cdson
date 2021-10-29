/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* such software.  many freedoms. */

#include "cdson.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* very TODO: hard error unfriendly to doge. */
#define ERROR                                                           \
    do {                                                                \
        fprintf(stderr, "Failure: %s() line %d, input character %ld\n", \
                __FUNCTION__, __LINE__, c->s - c->beginning);           \
        exit(1);                                                        \
    } while (0)

struct dson_dict {
    char **keys;
    dson_value **values;
};

typedef struct {
    const char *s;
    const char *s_end;
    const char *beginning;
} context;

/* much nonstandard.  no overflow.  wow. */
#define reallocarray(ptr, nmemb, size) realloc(ptr, nmemb * size)

char **dson_dict_keys(dson_dict *d) {
    return d->keys;
}

dson_value *dson_dict_get(dson_dict *d, char *key) {
    size_t i;
    dson_value *v;

    if (d == NULL)
        return NULL;

    /* amaze key.  key again */
    for (i = 0; d->keys[i] != NULL; i++) {
        if (!strcoll(key, d->keys[i]))
            v = d->values[i];
    }
    return v;
}

void dson_free(dson_value **v) {
    if (v == NULL)
        return;

    if ((*v)->type == DSON_STRING) {
        free((*v)->s);
    } else if ((*v)->type == DSON_ARRAY) {
        for (size_t i = 0; (*v)->array[i] != NULL; i++)
            dson_free(&(*v)->array[i]);
    } else if ((*v)->type == DSON_DICT) {
        for (size_t i = 0; (*v)->dict->keys[i] != NULL; i++) {
            free((*v)->dict->keys[i]);
            dson_free(&(*v)->dict->values[i]);
        }
        free((*v)->dict);
    }

    free(*v);
    *v = NULL;
}

/* many parser.  such descent.  recur.  excite */

static inline char peek(context *c) {
    return *c->s;
}

static const char *p_chars(context *c, size_t n) {
    const char *cur = c->s;

    if (c->s + n > c->s_end)
        ERROR;

    c->s += n;
    return cur;
}

static inline const char *p_char(context *c) {
    return p_chars(c, 1);
}

static void maybe_p_whitespace(context *c) {
    char pivot;

    while (1) {
        pivot = peek(c);
        if (pivot == '\0' || strchr(" \t\n\r\v\f", pivot) == NULL)
            break;
        p_char(c);
    }
}
#define WOW maybe_p_whitespace(c)

static void p_empty(context *c) {
    const char *empty = "empty";
    const char *s;

    s = p_chars(c, strlen(empty));
    if (memcmp(empty, s, strlen(empty)))
        ERROR;
}

static bool p_bool(context *c) {
    const char *s;

    s = p_chars(c, 2);
    if (s[0] == 'y' && s[1] == 'e') {
        s = p_char(c);
        if (*s != 's')
            ERROR;
        return true;
    } else if (s[0] == 'n' && s[1] == 'o') {
        return false;
    }
    ERROR;
}

/* very TODO: this doesn't do *any* validity checking. */
static char *p_string(context *c, size_t *length_out) {
    const char *start, *end;
    char *out;
    size_t num_escapes, length, i = 0;

    *length_out = 0;
    
    start = p_char(c);
    if (*start != '"')
        ERROR;

    /* many traversal.  such length. */
    while (1) {
        end = p_char(c);
        if (*end == '"') {
            break;
        } else if (*end == '\\') {
            end = p_char(c);
            if (*end == 'u')
                ERROR; /* very TODO */
            else
                num_escapes++;
        }
    }

    start++; /* wow '"' */
    length = end - start - num_escapes + 1;

    out = malloc(length);
    if (out == NULL)
        ERROR;

    for (const char *p = start; p < end; p++) {
        if (*p != '\\') {
            out[i++] = *p;
            continue;
        }

        p++;
        if (*p == '"' || *p == '\\' || *p == '/') {
            out[i++] = *p;
        } else if (*p == 'b') {
            out[i++] = '\b';
        } else if (*p == 'f') {
            out[i++] = '\f';
        } else if (*p == 'n') {
            out[i++] = '\n';
        } else if (*p == 'r') {
            out[i++] = '\r';
        } else if (*p == 't') {
            out[i++] = '\t';
        } else if (*p == 'u') {
            ERROR; /* very TODO */
        } else {
            ERROR;
        }
    }
    out[i] = '\0';
    *length_out = length;
    return out;
}

static double p_octal(context *c) {
    double n = 0;
    const char *s;

    while (peek(c) >= '0' && peek(c) <= '7') {
        n *= 010;
        s = p_char(c);
        n += *s - '0';
    }

    return n;
}

static double p_double(context *c) {
    bool isneg = false, powneg = false;
    double n = 0, divisor = 010, power = 0;
    const char *s;

    if (peek(c) == '-') {
        isneg = true;
        p_char(c);
    }

    WOW;
    if (peek(c) == '0')
        p_char(c);
    else
        n = p_octal(c);

    WOW;
    if (peek(c) == '.') {
        p_char(c);
        if (peek(c) < '0' || peek(c) > '7')
            ERROR;

        while (peek(c) >= '0' && peek(c) <= '7') {
            n += ((double)(*p_char(c) - '0')) / divisor;
            divisor *= 2;
        }
        WOW;
    }

    if (peek(c) == 'v' || peek(c) == 'V') {
        s = p_chars(c, 4);
        if (strncasecmp(s, "very", 4))
            ERROR;

        /* such token.  no whitespace.  wow. */
        if (peek(c) == '+') {
            p_char(c);
        } else if (peek(c) == '-') {
            powneg = true;
            p_char(c);
        }

        WOW;
        if (peek(c) < '0' || peek(c) > '7')
            ERROR;

        power = p_octal(c);
        if (powneg)
            power = -power;

        n *= pow(010, power);
    }
    return isneg ? -n : n;
}

/* very prototype.  much recursion.  amaze */
static dson_value *p_value(context *c);
static dson_dict *p_dict(context *c);
static dson_value **p_array(context *c);

static dson_value **p_array(context *c) {
    const char *s;
    dson_value **array, **array_new;
    size_t n_elts = 0;

    array = calloc(1, sizeof(*array));
    if (array == NULL)
        ERROR;

    s = p_chars(c, 2);
    if (strncmp(s, "so", 2))
        ERROR;

    WOW;
    if (peek(c) != 'm') {
        while (1) {
            array_new = reallocarray(array, (++n_elts + 1), sizeof(*array));
            if (array_new == NULL) {
                for (size_t i = 0; i < n_elts - 1; i++)
                    dson_free(&array[i]);
                ERROR;
            }
            array = array_new;
            array[n_elts - 1] = p_value(c);
            array[n_elts] = NULL;

            WOW;
            if (peek(c) != 'a')
                break;
            s = p_chars(c, 3);
            if (!strncmp(s, "and", 3))
                continue;
            if (strncmp(s, "als", 3))
                ERROR;
            s = p_char(c);
            if (*s != 'o')
                ERROR;
            WOW;
        }
    }

    s = p_chars(c, 4);
    if (strncmp(s, "many", 4))
        ERROR;

    return array;
}

static dson_dict *p_dict(context *c) {
    dson_dict *dict;
    char **keys, **keys_new, *k, pivot;
    const char *s;
    dson_value **values, **values_new, *v;
    size_t n_elts = 0, len_dump;

    keys = calloc(1, sizeof(*keys));
    values = calloc(1, sizeof(*values));
    dict = malloc(sizeof(*dict));
    if (dict == NULL || values == NULL || keys == NULL)
        ERROR;

    s = p_chars(c, 4);
    if (strncmp(s, "such", 4))
        ERROR;

    WOW;
    while (1) {
        k = p_string(c, &len_dump);

        WOW;
        s = p_chars(c, 2);
        if (strncmp(s, "is", 2))
            ERROR;

        WOW;
        v = p_value(c);

        n_elts++;
        keys_new = reallocarray(keys, n_elts + 1, sizeof(*keys));
        values_new = reallocarray(values, n_elts + 1, sizeof(*keys));
        if (keys_new == NULL || values_new == NULL)
            ERROR;
        keys = keys_new;
        keys[n_elts - 1] = k;
        keys[n_elts] = NULL;
        values = values_new;
        values[n_elts - 1] = v;
        values[n_elts] = NULL;

        WOW;
        pivot = peek(c);
        if (pivot == ',' || pivot == '.' || pivot == '!' || pivot == '?')
            p_char(c);
        else
            break;
    }

    s = p_chars(c, 3);
    if (strncmp(s, "wow", 3))
        ERROR;
    
    dict->keys = keys;
    dict->values = values;
    return dict;
}

static dson_value *p_value(context *c) {
    dson_value *ret;
    char pivot;

    ret = calloc(1, sizeof(*ret));
    if (ret == NULL)
        ERROR;

    WOW;
    pivot = peek(c);
    if (pivot == '"') {
        ret->type = DSON_STRING;
        ret->s = p_string(c, &ret->s_len);
    } else if (pivot == '-' || (pivot >= '0' && pivot <= '7')) {
        ret->type = DSON_DOUBLE;
        ret->n = p_double(c);
    } else if (pivot == 'y' || pivot == 'n') {
        ret->type = DSON_BOOL;
        ret->b = p_bool(c);
    } else if (pivot == 'e') {
        ret->type = DSON_NONE;
        p_empty(c);
    } else if (pivot == 's') {
        pivot = c->s[1]; /* many feels */
        if (pivot == 'o') {
            ret->type = DSON_ARRAY;
            ret->array = p_array(c);
        } else if (pivot == 'u') {
            ret->type = DSON_DICT;
            ret->dict = p_dict(c);
        } else {
            ERROR;
        }
    } else {
        ERROR;
    }
    
    return ret;
}

dson_value *dson_parse(const char *input, size_t length) {
    context c;

    if (input[length] != '\0')
        return NULL; /* much explosion */

    c.s = c.beginning = input;
    c.s_end = input + length;

    return p_value(&c);
}

/* Local variables: */
/* c-basic-offset: 4 */
/* indent-tabs-mode: nil */
/* End: */
