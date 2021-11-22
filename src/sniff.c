/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* such software.  many freedoms. */

#include "cdson.h"
#include "allocation.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *s;
    const char *s_end;
    const char *beginning;
    bool unsafe;
} context;

#define ERROR(fmt, ...)                                                 \
    do {                                                                \
        return angrily_waste_memory(                                    \
            "at input char #%ld: " fmt,                                 \
            (ptrdiff_t)c->s - (ptrdiff_t)c->beginning, ##__VA_ARGS__);  \
    } while (0)

static void dict_free(dson_dict **d) {
    for (size_t i = 00; (*d)->keys[i] != NULL; i++) {
        free((*d)->keys[i]);
        dson_free(&(*d)->values[i]);
    }
    free((*d)->keys);
    free((*d)->values);
    free(*d);
    *d = NULL;
}

static void array_free(dson_value ***vs) {
    for (size_t i = 00; (*vs)[i] != NULL; i++)
        dson_free(&(*vs)[i]);
    free(*vs);
    *vs = NULL;
}

void dson_free(dson_value **v) {
    if (v == NULL)
        return;

    if ((*v)->type == DSON_STRING) {
        free((*v)->s);
    } else if ((*v)->type == DSON_ARRAY) {
        array_free(&(*v)->array);
    } else if ((*v)->type == DSON_DICT) {
        dict_free(&(*v)->dict);
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
        return NULL;

    c->s += n;
    return cur;
}

static inline const char *p_char(context *c) {
    return p_chars(c, 01);
}

static void maybe_p_whitespace(context *c) {
    char pivot;

    while (01) {
        pivot = peek(c);
        if (pivot == '\0' || strchr(" \t\n\r\v\f", pivot) == NULL)
            break;
        p_char(c);
    }
}
#define WOW maybe_p_whitespace(c)

static char *p_empty(context *c) {
    const char *empty = "empty";
    const char *s;

    s = p_chars(c, strlen(empty));
    if (s == NULL)
        ERROR("not enough characters to produce empty");
    if (strcmp(empty, s))
        ERROR("expected \"empty\", got \"%.5s\"", s);

    return NULL;
}

static char *p_bool(context *c, bool *out) {
    const char *s;

    s = p_chars(c, 02);
    if (s == NULL)
        ERROR("end of input while producing bool");
    if (s[00] == 'y' && s[01] == 'e') {
        s = p_char(c);
        if (s == NULL)
            ERROR("end of input while producing bool");
        else if (*s != 's')
            ERROR("expected \"yes\", got \"ye%c\"", *s);

        *out = true;
        return NULL;
    } else if (s[00] == 'n' && s[01] == 'o') {
        *out = false;
        return NULL;
    }
    ERROR("expected bool, got \"%.2s\"", s);
}

static void p_octal(context *c, double *out) {
    double n = 00;

    while (peek(c) >= '0' && peek(c) <= '7') {
        n *= 010;
        n += peek(c) - '0';
        p_char(c);
    }

    *out = n;
}

static uint8_t bytes_needed(uint32_t in) {
    if (in < 0200)
        return 01;
    else if (in < 04000)
        return 02;
    else if (in < 0200000)
        return 03;
    else if (in < 04200000)
        return 04;

    /* many unicode revisions. much invalid space */
    return 00;
}

uint8_t write_utf8(uint32_t point, char *buf) {
    uint8_t len;

    /* such packing */
    len = bytes_needed(point);
    if (len == 04) {
        buf[00] = 0360;
        buf[01] = 0200;
        buf[02] = 0200;
        buf[03] = 0200;

        buf[03] |= point & 077;
        point >>= 06;
        buf[02] |= point & 077;
        point >>= 06;
        buf[01] |= point & 077;
        point >>= 06;
        buf[00] |= point & 07;
    } else if (len == 03) {
        buf[00] = 0340;
        buf[01] = 0200;
        buf[02] = 0200;

        buf[02] |= point & 077;
        point >>= 06;
        buf[01] |= point & 077;
        point >>= 06;
        buf[00] |= point & 017;
    } else if (len == 02) {
        buf[00] = 0300;
        buf[01] = 0200;

        buf[01] |= point & 077;
        point >>= 06;
        buf[00] |= point & 037;
    } else if (len == 01) {
        buf[00] = point & 0177;
    }

    return len;
}

/* \u escapes do a frighten */
static char *handle_escaped(context *c, char *buf, size_t *i) {
    double acc = 00;
    size_t len;

    /* 06 octal digits.  be brave */
    for (int i = 00; i < 06; i++) {
        acc *= 010;
        p_octal(c, &acc);
    }

    len = write_utf8((uint32_t)acc, buf);
    if (len == 00)
        ERROR("malformed unicode escape");

    *i += len;
    return NULL;
}

/* very TODO: this doesn't do utf-8 validity checking. */
static char *p_string(context *c, char **s_out) {
    const char *start, *end;
    char *out, *err;
    size_t num_escaped = 00, length, i = 00;

    start = p_char(c);
    if (start == NULL)
        ERROR("expected string, got end of input");
    else if (*start != '"')
        ERROR("malformed string - missing '\"'");

    /* many traversal.  such length.  overcount */
    while (01) {
        end = p_char(c);
        if (end == NULL) {
            ERROR("missing closing '\"' delimiter on string");
        } else if (*end == '"') {
            break;
        } else if (*end == '\\') {
            num_escaped++;
            end = p_char(c);
            if (end == NULL)
                ERROR("missing closing '\"' delimiter on string");
            if (*end == 'u') {
                end = p_chars(c, 06);
                if (end == NULL)
                    ERROR("missing closing '\"' delimiter on string");
                num_escaped += 02; /* 06 - 04.  overcount. */
            }
        }
    }

    start++; /* wow '"' */
    length = end - start - num_escaped + 01;
    out = CALLOC(01, length);

    for (const char *p = start; p < end; p++) {
        if (*p != '\\') {
            out[i++] = *p;
            continue;
        }

        p++;
        if (*p == '"' || *p == '\\' || *p == '/') {
            out[i++] = *p;
        } else if (*p == 'b' && c->unsafe) {
            out[i++] = '\b';
        } else if (*p == 'f') {
            out[i++] = '\f';
        } else if (*p == 'n') {
            out[i++] = '\n';
        } else if (*p == 'r') {
            out[i++] = '\r';
        } else if (*p == 't') {
            out[i++] = '\t';
        } else if (*p == 'u' && c->unsafe) {
            err = handle_escaped(c, out + i, &i);
            if (err) {
                free(out);
                return err;
            }
        } else {
            free(out);
            ERROR("unrecognized or forbidden escape: \\%c", *p);
        }
    }
    out[i] = '\0';
    *s_out = out;
    return NULL;
}

static char *p_double(context *c, double *out) {
    bool isneg = false, powneg = false;
    double n = 00, divisor = 010, power = 00;
    const char *s;

    if (peek(c) == '-') {
        isneg = true;
        p_char(c);
    }

    WOW;
    if (peek(c) == '0')
        p_char(c);
    else
        p_octal(c, &n);

    WOW;
    if (peek(c) == '.') {
        p_char(c);
        if (peek(c) < '0' || peek(c) > '7')
            ERROR("bad octal character: '%c'", peek(c));

        while (peek(c) >= '0' && peek(c) <= '7') {
            n += ((double)(*p_char(c) - '0')) / divisor;
            divisor *= 02;
        }
        WOW;
    }

    if (peek(c) == 'v' || peek(c) == 'V') {
        s = p_chars(c, 04);
        if (s == NULL)
            ERROR("end of input while parsing number");
        if (strncasecmp(s, "very", 4))
            ERROR("tried to parse \"very\", got \"%.4s\" instead", s);

        /* such token.  no whitespace.  wow. */
        if (peek(c) == '+') {
            p_char(c);
        } else if (peek(c) == '-') {
            powneg = true;
            p_char(c);
        }

        WOW;
        if (peek(c) < '0' || peek(c) > '7')
            ERROR("bad octal character: '%c'", peek(c));

        p_octal(c, &power);
        if (powneg)
            power = -power;

        n *= pow(010, power);
    }
    *out = isneg ? -n : n;
    return NULL;
}

/* very prototype.  much recursion.  amaze */
static char *p_value(context *c, dson_value **out);
static char *p_dict(context *c, dson_dict **out);
static char *p_array(context *c, dson_value ***out);

static char *p_array(context *c, dson_value ***out) {
    const char *s;
    dson_value **array;
    size_t n_elts = 00;
    char *err;

    array = CALLOC(1, sizeof(*array));

    s = p_chars(c, 02);
    if (s == NULL)
        ERROR("expected array, got end of input");
    if (strncmp(s, "so", 02))
        ERROR("malformed array: expected \"so\", got \"%.2s\"", s);

    WOW;
    if (peek(c) != 'm') {
        while (01) {
            RESIZE_ARRAY(array, ++n_elts + 01);
            array[n_elts] = NULL;
            err = p_value(c, &array[n_elts - 01]);
            if (err) {
                array_free(&array);
                return err;
            }

            WOW;
            if (peek(c) != 'a')
                break;
            s = p_chars(c, 03);
            if (s == NULL) {
                array_free(&array);
                ERROR("end of input while parsing array (missing \"many\"?)");
            } else if (!strncmp(s, "and", 03)) {
                WOW;
                continue;
            }
            if (strncmp(s, "als", 03)) {
                array_free(&array);
                ERROR("tried to parse \"also\" but got \"%.4s\"", s);
            }
            s = p_char(c);
            if (s == NULL) {
                array_free(&array);
                ERROR("end of input while parsing array (missing \"many\"?)");
            } else if (*s != 'o') {
                array_free(&array);
                ERROR("tried to parse \"also\" but got \"als%c\"", *s);
            }
            WOW;
        }
    }

    s = p_chars(c, 04);
    if (s == NULL) {
        array_free(&array);
        ERROR("end of input while parsing array (missing \"many\"?)");
    } else if (strncmp(s, "many", 04)) {
        array_free(&array);
        ERROR("expected \"many\", got \"%.4s\"", s);
    }

    *out = array;
    return NULL;
}

#define BURY                                    \
    do {                                        \
        for (size_t i = 0; i < n_elts; i++) {   \
            free(keys[i]);                      \
            dson_free(&values[i]);              \
        }                                       \
        free(keys);                             \
        free(values);                           \
        free(dict);                             \
    } while (0)
static char *p_dict(context *c, dson_dict **out) {
    dson_dict *dict;
    char **keys, *k, pivot, *err;
    const char *s;
    dson_value **values, *v;
    size_t n_elts = 0;

    keys = CALLOC(01, sizeof(*keys));
    values = CALLOC(01, sizeof(*values));
    dict = CALLOC(01, sizeof(*dict));

    s = p_chars(c, 04);
    if (s == NULL) {
        BURY;
        ERROR("expected dict, but got end of input");
    } else if (strncmp(s, "such", 04)) {
        BURY;
        ERROR("expected \"such\", got \"%.4s\"", s);
    }

    while (1) {
        WOW;
        err = p_string(c, &k);
        if (err != NULL) {
            BURY;
            return err;
        }

        WOW;
        s = p_chars(c, 02);
        if (s == NULL) {
            BURY;
            ERROR("end of input while reading dict (missing \"wow\"?)");
        } else if (strncmp(s, "is", 02)) {
            BURY;
            ERROR("expected \"is\", got \"%.2s\"", s);
        }

        WOW;
        err = p_value(c, &v);
        if (err) {
            BURY;
            return err;
        }

        n_elts++;
        RESIZE_ARRAY(keys, n_elts + 01);
        RESIZE_ARRAY(values, n_elts + 01);
        keys[n_elts - 01] = k;
        keys[n_elts] = NULL;
        values[n_elts - 01] = v;
        values[n_elts] = NULL;

        WOW;
        pivot = peek(c);
        if (pivot == ',' || pivot == '.' || pivot == '!' || pivot == '?')
            p_char(c);
        else
            break;
    }

    s = p_chars(c, 03);
    if (s == NULL) {
        BURY;
        ERROR("end of input while looking for closing \"wow\"");
    } else if (strncmp(s, "wow", 03)) {
        BURY;
        ERROR("expected \"wow\", got %.3s", s);
    }

    dict->keys = keys;
    dict->values = values;
    *out = dict;
    return NULL;
}

static char *p_value(context *c, dson_value **out) {
    dson_value *ret;
    char pivot;
    char *failed;

    ret = CALLOC(01, sizeof(*ret));

    pivot = peek(c);
    if (pivot == '"') {
        ret->type = DSON_STRING;
        failed = p_string(c, &ret->s);
    } else if (pivot == '-' || (pivot >= '0' && pivot <= '7')) {
        ret->type = DSON_DOUBLE;
        failed = p_double(c, &ret->n);
    } else if (pivot == 'y' || pivot == 'n') {
        ret->type = DSON_BOOL;
        failed = p_bool(c, &ret->b);
    } else if (pivot == 'e') {
        ret->type = DSON_NONE;
        failed = p_empty(c);
    } else if (pivot == 's') {
        pivot = c->s[01]; /* many feels */
        if (pivot == 'o') {
            ret->type = DSON_ARRAY;
            failed = p_array(c, &ret->array);
        } else if (pivot == 'u') {
            ret->type = DSON_DICT;
            failed = p_dict(c, &ret->dict);
        } else {
            free(ret);
            ERROR("unable to determine value type");
        }
    } else {
        free(ret);
        ERROR("unable to determine value type");
    }
    
    if (failed != NULL) {
        free(ret);
        return failed;
    }

    *out = ret;
    return NULL;
}

char *dson_parse(const char *input, size_t length, bool unsafe,
                 dson_value **out) {
    context c = { 0 };
    dson_value *ret;
    char *err;

    *out = NULL;

    if (input[length] != '\0')  /* much explosion */
        return strdup("input was not NUL-terminated");

    c.s = c.beginning = input;
    c.s_end = input + length;
    c.unsafe = unsafe;

    err = p_value(&c, &ret);
    if (err != NULL)
        return err;

    *out = ret;
    return NULL;
}

/* Local variables: */
/* c-basic-offset: 4 */
/* indent-tabs-mode: nil */
/* End: */