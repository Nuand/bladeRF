#include "bladeRF_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --- Helper functions --- */
static void skip_ws(const char **p)
{
    while (**p && isspace((unsigned char)**p)) {
        (*p)++;
    }
}

static char *parse_string(const char **p)
{
    if (**p != '"') {
        return NULL;
    }
    (*p)++; // skip opening quote
    const char *start = *p;
    char *out = malloc(strlen(start) + 1); // worst case
    if (!out) return NULL;
    char *dst = out;
    while (**p && **p != '"') {
        if (**p == '\\') { // escape
            (*p)++;
            char c = **p;
            if (c == '"' || c == '\\' || c == '/') {
                *dst++ = c;
            } else if (c == 'b') {
                *dst++ = '\b';
            } else if (c == 'f') {
                *dst++ = '\f';
            } else if (c == 'n') {
                *dst++ = '\n';
            } else if (c == 'r') {
                *dst++ = '\r';
            } else if (c == 't') {
                *dst++ = '\t';
            } else if (c == 'u') {
                // skip unicode sequence (unhandled) - just copy as ?
                // expect 4 hex digits
                for (int i=0;i<4 && *(*p+1+i);i++);
                *dst++ = '?';
            } else {
                // invalid escape, copy as-is
                *dst++ = c;
            }
        } else {
            *dst++ = **p;
        }
        (*p)++;
    }
    if (**p != '"') {
        free(out);
        return NULL; // unterminated
    }
    (*p)++; // skip closing quote
    *dst = '\0';
    return out;
}

static json_object *new_string_value(char *str)
{
    json_object *v = malloc(sizeof(*v));
    if (!v) return NULL;
    v->type = BLADERF_JSON_STRING;
    v->u.string = str;
    return v;
}

static json_object *new_object_value(struct json_kv *kv)
{
    json_object *v = malloc(sizeof(*v));
    if (!v) return NULL;
    v->type = BLADERF_JSON_OBJECT;
    v->u.object = kv;
    return v;
}

static struct json_kv *parse_object(const char **p);

static json_object *parse_value(const char **p)
{
    skip_ws(p);
    if (**p == '"') {
        char *s = parse_string(p);
        if (!s) return NULL;
        return new_string_value(s);
    } else if (**p == '{') {
        (*p)++; // consume '{'
        struct json_kv *kv = parse_object(p);
        if (!kv) return NULL;
        return new_object_value(kv);
    }

    return NULL; // unsupported type
}

static struct json_kv *parse_object(const char **p)
{
    skip_ws(p);
    if (**p == '}') { // empty object
        (*p)++;
        return NULL;
    }

    struct json_kv *head = NULL;
    struct json_kv *tail = NULL;

    while (1) {
        skip_ws(p);
        char *key = parse_string(p);
        if (!key) goto error;
        skip_ws(p);
        if (**p != ':') goto error;
        (*p)++; // consume ':'
        json_object *val = parse_value(p);
        if (!val) goto error;

        struct json_kv *node = malloc(sizeof(*node));
        if (!node) goto error;
        node->key = key;
        node->value = val;
        node->next = NULL;
        if (!head) head = node; else tail->next = node;
        tail = node;

        skip_ws(p);
        if (**p == ',') {
            (*p)++; // another pair
            continue;
        } else if (**p == '}') {
            (*p)++; // end object
            break;
        } else {
            goto error;
        }
    }

    return head;

error:
    // free partially built object
    while (head) {
        struct json_kv *n = head->next;
        free(head->key);
        bladerf_json_free(head->value);
        free(head);
        head = n;
    }
    return NULL;
}

/* --- Public API implementations --- */
json_object *bladerf_json_parse(const char *input)
{
    const char *p = input;
    skip_ws(&p);
    if (*p != '{') {
        return NULL;
    }
    p++; // consume '{'
    struct json_kv *kv = parse_object(&p);
    if (!kv) return NULL;
    json_object *root = new_object_value(kv);
    return root;
}

static void free_kv(struct json_kv *kv)
{
    while (kv) {
        struct json_kv *next = kv->next;
        free(kv->key);
        bladerf_json_free(kv->value);
        free(kv);
        kv = next;
    }
}

void bladerf_json_free(json_object *obj)
{
    if (!obj) return;
    if (obj->type == BLADERF_JSON_STRING) {
        free(obj->u.string);
    } else if (obj->type == BLADERF_JSON_OBJECT) {
        free_kv(obj->u.object);
    }
    free(obj);
}

int bladerf_json_object_get_ex(const json_object *obj, const char *key, json_object **value_out)
{
    if (!obj || obj->type != BLADERF_JSON_OBJECT) return 0;
    struct json_kv *kv = obj->u.object;
    while (kv) {
        if (strcmp(kv->key, key) == 0) {
            if (value_out) *value_out = kv->value;
            return 1;
        }
        kv = kv->next;
    }
    return 0;
}

int bladerf_json_object_length(const json_object *obj)
{
    if (!obj || obj->type != BLADERF_JSON_OBJECT) return 0;
    int count = 0;
    struct json_kv *kv = obj->u.object;
    while (kv) { count++; kv = kv->next; }
    return count;
}

const char *bladerf_json_get_string(const json_object *obj)
{
    if (!obj || obj->type != BLADERF_JSON_STRING) return NULL;
    return obj->u.string;
}
