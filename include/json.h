#ifndef JSON_H
#define JSON_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── JSON Value Types ───────────────────────────────────────── */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

/* ── Forward declarations ───────────────────────────────────── */
typedef struct JsonValue JsonValue;
typedef struct JsonPair  JsonPair;

struct JsonValue {
    JsonType   type;
    bool       bool_val;
    double     num_val;
    char*      str_val;       /* owned, freed by parent */
    size_t     size;          /* for array / object */
    JsonValue* children;      /* array elements OR object members */
    char*      key;           /* for object members (string key) */
};

/* ── Value construction ─────────────────────────────────────── */
JsonValue* json_null_val(void);
JsonValue* json_bool_val(bool value);
JsonValue* json_num_val(double value);
JsonValue* json_str_val(const char* s);
JsonValue* json_arr_val(void);
JsonValue* json_obj_val(void);

/* ── Mutators ───────────────────────────────────────────────── */
void json_append  (JsonValue* arr, JsonValue* child);
void json_set     (JsonValue* obj, const char* key, JsonValue* val);
JsonValue* json_get(const JsonValue* obj, const char* key);

/* Deep clone */
JsonValue* json_clone(const JsonValue* v);

/* ── Serialisation ──────────────────────────────────────────── */
char* json_to_string(const JsonValue* root, bool pretty);
size_t json_to_buffer(const JsonValue* root, char* buf, size_t buflen, bool pretty);

/* ── Parsing ────────────────────────────────────────────────── */
JsonValue* json_parse(const char* input);

/* ── Destruction ────────────────────────────────────────────── */
void json_free(JsonValue* root);

/* ── Accessors (const-safe) ─────────────────────────────────── */
JsonType json_type(const JsonValue* v);
bool     json_is_null(const JsonValue* v);
bool     json_is_bool(const JsonValue* v);
bool     json_is_number(const JsonValue* v);
bool     json_is_string(const JsonValue* v);
bool     json_is_array(const JsonValue* v);
bool     json_is_object(const JsonValue* v);

bool json_get_bool(const JsonValue* v);
double json_get_number(const JsonValue* v);
const char* json_get_string(const JsonValue* v);
size_t json_arr_size(const JsonValue* arr);
size_t json_obj_size(const JsonValue* obj);
const JsonValue* json_arr_at(const JsonValue* arr, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* JSON_H */
