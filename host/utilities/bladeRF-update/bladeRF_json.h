#ifndef BLADERF_JSON_H
#define BLADERF_JSON_H

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Minimal JSON implementation for bladeRF-update utility.
 * Supports: objects {"key":"value", ...}, nested objects, and
 * string scalar values. No arrays, numbers, booleans, or null.
 */

typedef enum {
    BLADERF_JSON_STRING,
    BLADERF_JSON_OBJECT
} bladerf_json_type;

struct json_object; /* forward declaration */
struct json_kv; /* forward */

typedef struct json_object {
    bladerf_json_type type;
    union {
        char               *string;  /* BLADERF_JSON_STRING */
        struct json_kv     *object;  /* BLADERF_JSON_OBJECT (linked list) */
    } u;
} json_object;

/* Key/value linked list for objects */
struct json_kv {
    char                *key;
    struct json_object *value;
    struct json_kv      *next;
};

/* Function prototypes (prefixed to avoid collision) */
json_object *bladerf_json_parse(const char *input);
void         bladerf_json_free(json_object *obj);
int          bladerf_json_object_get_ex(const json_object *obj, const char *key, json_object **value_out);
int          bladerf_json_object_length(const json_object *obj);
const char  *bladerf_json_get_string(const json_object *obj);

/* Iterator helper */
static inline struct json_kv *bladerf_json_object_iter(const json_object *obj)
{
    if (!obj || obj->type != BLADERF_JSON_OBJECT) {
        return NULL;
    }
    return obj->u.object;
}

/* Macros to provide json-c compatible API subset */
#define json_tokener_parse(str)              bladerf_json_parse(str)
#define json_object_put(obj)                 bladerf_json_free(obj)
#define json_object_object_get_ex(obj,k,v)   bladerf_json_object_get_ex(obj,k,v)
#define json_object_object_length(obj)       bladerf_json_object_length(obj)
#define json_object_get_string(obj)          bladerf_json_get_string(obj)

/* Foreach macro mimicking json-c behaviour */
#define json_object_object_foreach(obj, jkey, jval)                         \
    char *jkey;                                                             \
    json_object *jval;                                                      \
    for (struct json_kv *__kv = bladerf_json_object_iter(obj);              \
         __kv != NULL && (((jkey) = __kv->key), ((jval) = __kv->value), 1); \
         __kv = __kv->next)


#ifdef __cplusplus
}
#endif

#endif /* BLADERF_JSON_H */