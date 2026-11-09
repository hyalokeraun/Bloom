#define _CRT_SECURE_NO_WARNINGS
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

static char* str_dup(const char* s);
static void json_free_children(JsonValue* v);

static char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* d = (char*)malloc(len + 1);
    memcpy(d, s, len);
    d[len] = '\0';
    return d;
}

JsonValue* json_null_val(void) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    v->type = JSON_NULL;
    return v;
}
JsonValue* json_bool_val(bool value) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    v->type = JSON_BOOL;
    v->bool_val = value ? 1 : 0;
    return v;
}
JsonValue* json_num_val(double value) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    v->type = JSON_NUMBER;
    v->num_val = value;
    return v;
}
JsonValue* json_str_val(const char* s) {
    if (!s) return json_null_val();
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    v->type = JSON_STRING;
    v->str_val = str_dup(s);
    return v;
}
JsonValue* json_arr_val(void) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    v->type = JSON_ARRAY;
    return v;
}
JsonValue* json_obj_val(void) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    v->type = JSON_OBJECT;
    return v;
}

void json_append(JsonValue* arr, JsonValue* child) {
    if (!arr || !child || arr->type != JSON_ARRAY) return;
    arr->children = (JsonValue*)realloc(arr->children, (arr->size + 1) * sizeof(JsonValue));
    arr->children[arr->size] = *json_clone(child);
    arr->children[arr->size].key = NULL;
    arr->size++;
}

void json_set(JsonValue* obj, const char* key, JsonValue* val) {
    if (!obj || !key || !val || obj->type != JSON_OBJECT) return;
    for (size_t i = 0; i < obj->size; i++) {
        if (strcmp(obj->children[i].key, key) == 0) {
            json_free_children(&obj->children[i]);
            obj->children[i] = *json_clone(val);
            obj->children[i].key = str_dup(key);
            return;
        }
    }
    obj->children = (JsonValue*)realloc(obj->children, (obj->size + 1) * sizeof(JsonValue));
    obj->children[obj->size] = *json_clone(val);
    obj->children[obj->size].key = str_dup(key);
    obj->size++;
}

JsonValue* json_get(const JsonValue* obj, const char* key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < obj->size; i++)
        if (strcmp(obj->children[i].key, key) == 0) return &obj->children[i];
    return NULL;
}

JsonValue* json_clone(const JsonValue* v) {
    if (!v) return NULL;
    JsonValue* c = (JsonValue*)calloc(1, sizeof(JsonValue));
    *c = *v;
    if (v->type == JSON_STRING && v->str_val) c->str_val = str_dup(v->str_val);
    if ((v->type == JSON_OBJECT || v->type == JSON_ARRAY) && v->children && v->size) {
        c->children = (JsonValue*)calloc(v->size, sizeof(JsonValue));
        for (size_t i = 0; i < v->size; i++) {
            c->children[i] = *json_clone(&v->children[i]);
            if (v->children[i].key) c->children[i].key = str_dup(v->children[i].key);
        }
    }
    return c;
}
static size_t json_escape(const char* s, size_t slen, char* buf, size_t buflen) {
    size_t j = 0;
    for (size_t i = 0; i < slen && j + 6 < buflen; i++) {
        switch (s[i]) {
            case '"':  buf[j++] = '\\'; buf[j++] = '"'; break;
            case '\\': buf[j++] = '\\'; buf[j++] = '\\'; break;
            case '\b': buf[j++] = '\\'; buf[j++] = 'b'; break;
            case '\f': buf[j++] = '\\'; buf[j++] = 'f'; break;
            case '\n': buf[j++] = '\\'; buf[j++] = 'n'; break;
            case '\r': buf[j++] = '\\'; buf[j++] = 'r'; break;
            case '\t': buf[j++] = '\\'; buf[j++] = 't'; break;
            default:
                if ((unsigned char)s[i] < 0x20) {
                    j += (size_t)snprintf(buf + j, buflen - j, "\\u%04x", (unsigned char)s[i]);
                } else {
                    buf[j++] = s[i];
                }
                break;
        }
    }
    buf[j] = '\0';
    return j;
}

static size_t json_serialize(const JsonValue* v, char* buf, size_t buflen, int indent, int depth, bool pretty) {
    size_t pos = 0;
    char pad[128];
    if (pretty) { int p = depth * indent; if (p > 120) p = 120; memset(pad, ' ', (size_t)p); pad[p] = '\0'; }
    switch (v->type) {
        case JSON_NULL: pos += (size_t)snprintf(buf + pos, buflen - pos, "null"); break;
        case JSON_BOOL: pos += (size_t)snprintf(buf + pos, buflen - pos, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
                pos += (size_t)snprintf(buf + pos, buflen - pos, "%lld", (long long)v->num_val);
            else pos += (size_t)snprintf(buf + pos, buflen - pos, "%.17g", v->num_val);
            break;
        case JSON_STRING:
            pos += (size_t)snprintf(buf + pos, buflen - pos, "\"");
            { char* escaped = (char*)malloc(8192);
              if (escaped) { json_escape(v->str_val, strlen(v->str_val), escaped, 8192);
                pos += (size_t)snprintf(buf + pos, buflen - pos, "%s", escaped); }
              free(escaped); }
            pos += (size_t)snprintf(buf + pos, buflen - pos, "\""); break;
        case JSON_ARRAY:
            if (v->size == 0) { pos += (size_t)snprintf(buf + pos, buflen - pos, "[]"); }
            else if (!pretty) {
                pos += (size_t)snprintf(buf + pos, buflen - pos, "[");
                for (size_t i = 0; i < v->size; i++) {
                    if (i) pos += (size_t)snprintf(buf + pos, buflen - pos, ",");
                    pos += json_serialize(&v->children[i], buf + pos, buflen - pos, indent, depth, pretty);
                } pos += (size_t)snprintf(buf + pos, buflen - pos, "]");
            } else {
                pos += (size_t)snprintf(buf + pos, buflen - pos, "[\n");
                for (size_t i = 0; i < v->size; i++) {
                    pos += (size_t)snprintf(buf + pos, buflen - pos, "%s  ", pad);
                    pos += json_serialize(&v->children[i], buf + pos, buflen - pos, indent, depth + 1, pretty);
                    if (i + 1 < v->size) pos += (size_t)snprintf(buf + pos, buflen - pos, ",");
                    pos += (size_t)snprintf(buf + pos, buflen - pos, "\n");
                } pos += (size_t)snprintf(buf + pos, buflen - pos, "%s]", pad);
            } break;
        case JSON_OBJECT:
            if (v->size == 0) { pos += (size_t)snprintf(buf + pos, buflen - pos, "{}"); }
            else if (!pretty) {
                pos += (size_t)snprintf(buf + pos, buflen - pos, "{");
                for (size_t i = 0; i < v->size; i++) {
                    if (i) pos += (size_t)snprintf(buf + pos, buflen - pos, ",");
                    { char* escaped = (char*)malloc(8192);
                      if (escaped) { json_escape(v->children[i].key, strlen(v->children[i].key), escaped, 8192);
                        pos += (size_t)snprintf(buf + pos, buflen - pos, "\"%s\":", escaped); }
                      free(escaped); }
                    pos += json_serialize(&v->children[i], buf + pos, buflen - pos, indent, depth, pretty);
                } pos += (size_t)snprintf(buf + pos, buflen - pos, "}");
            } else {
                pos += (size_t)snprintf(buf + pos, buflen - pos, "{\n");
                for (size_t i = 0; i < v->size; i++) {
                    char* escaped = (char*)malloc(8192);
                    if (escaped) {
                        json_escape(v->children[i].key, strlen(v->children[i].key), escaped, 8192);
                        pos += (size_t)snprintf(buf + pos, buflen - pos, "%s  \"%s\": ", pad, escaped);
                    }
                    free(escaped);
                    pos += json_serialize(&v->children[i], buf + pos, buflen - pos, indent, depth + 1, pretty);
                    if (i + 1 < v->size) pos += (size_t)snprintf(buf + pos, buflen - pos, ",");
                    pos += (size_t)snprintf(buf + pos, buflen - pos, "\n");
                } pos += (size_t)snprintf(buf + pos, buflen - pos, "%s}", pad);
            } break;
    }
    return pos;
}

char* json_to_string(const JsonValue* root, bool pretty) {
    if (!root) return str_dup("null");
    size_t estimated = json_obj_size(root) * 64 + 256;
    size_t buf_size = estimated > 8192 ? estimated : 8192;
    char* buf = (char*)malloc(buf_size);
    if (!buf) return str_dup("null");
    size_t len = json_serialize(root, buf, buf_size, 2, 0, pretty);
    if (len >= buf_size) {
        /* Estimate was too small, retry with exact size */
        char* larger = (char*)realloc(buf, len + 1);
        if (larger) {
            buf = larger;
            json_serialize(root, buf, len + 1, 2, 0, pretty);
        }
    }
    buf[len] = '\0';
    return buf;
}

size_t json_to_buffer(const JsonValue* root, char* buf, size_t buflen, bool pretty) {
    if (!root) { snprintf(buf, buflen, "null"); return 4; }
    return json_serialize(root, buf, buflen, 2, 0, pretty);
}

typedef struct { const char* data; size_t pos; size_t len; } Parser;
static void skip_ws(Parser* p) { while (p->pos < p->len && isspace((unsigned char)p->data[p->pos])) p->pos++; }
static char peek(const Parser* p) { skip_ws((Parser*)p); return p->pos < p->len ? p->data[p->pos] : '\0'; }
static char adv(Parser* p) { skip_ws(p); if (p->pos >= p->len) return '\0'; return p->data[p->pos++]; }
static bool mch(Parser* p, const char* s) { size_t slen = strlen(s); if (p->pos + slen > p->len) return 0; if (memcmp(p->data + p->pos, s, slen) != 0) return 0; p->pos += slen; return 1; }

static char* parse_string_val(Parser* p) {
    if (adv(p) != '"') return NULL;
    char* buf = (char*)malloc(p->len + 1); size_t j = 0;
    while (p->pos < p->len) {
        char c = p->data[p->pos++];
        if (c == '"') { buf[j] = '\0'; char* r = (char*)realloc(buf, j + 1); return r; }
        if (c == '\\') {
            if (p->pos >= p->len) { free(buf); return NULL; }
            char esc = p->data[p->pos++];
            switch (esc) {
                case '"': buf[j++] = '"'; break; case '\\': buf[j++] = '\\'; break;
                case '/': buf[j++] = '/'; break; case 'b': buf[j++] = '\b'; break;
                case 'f': buf[j++] = '\f'; break; case 'n': buf[j++] = '\n'; break;
                case 'r': buf[j++] = '\r'; break; case 't': buf[j++] = '\t'; break;
                case 'u': {
                    char hex[5] = {0};
                    for (int i = 0; i < 4 && p->pos < p->len; i++, p->pos++) hex[i] = p->data[p->pos];
                    unsigned int cp = (unsigned int)strtol(hex, NULL, 16);
                    if (cp < 0x80) buf[j++] = (char)cp;
                    else if (cp < 0x800) { buf[j++] = (char)(0xC0 | (cp >> 6)); buf[j++] = (char)(0x80 | (cp & 0x3F)); }
                    else { buf[j++] = (char)(0xE0 | (cp >> 12)); buf[j++] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[j++] = (char)(0x80 | (cp & 0x3F)); }
                    break; }
                default: buf[j++] = esc; break;
            }
        } else buf[j++] = c;
    }
    free(buf); return NULL;
}

static JsonValue* parse_value(Parser* p) {
    char c = peek(p);
    if (c == '\0') return NULL;
    if (c == '"') { char* s = parse_string_val(p); JsonValue* v = json_str_val(s); free(s); return v; }
    if (c == '-' || isdigit((unsigned char)c)) {
        skip_ws(p); char* end; double num = strtod(p->data + p->pos, &end);
        p->pos = (size_t)(end - p->data); return json_num_val(num);
    }
    if (mch(p, "true")) return json_bool_val(1);
    if (mch(p, "false")) return json_bool_val(0);
    if (mch(p, "null")) return json_null_val();
    if (c == '[') {
        adv(p); JsonValue* arr = json_arr_val();
        if (peek(p) != ']') {
            do { JsonValue* elem = parse_value(p); if (!elem) { json_free(arr); return NULL; } json_append(arr, elem); }
            while (adv(p) == ',');
        }
        if (adv(p) != ']') { json_free(arr); return NULL; }
        return arr;
    }
    if (c == '{') {
        adv(p); JsonValue* obj = json_obj_val();
        if (peek(p) != '}') {
            do {
                char* key = parse_string_val(p); if (!key) { json_free(obj); return NULL; }
                if (adv(p) != ':') { free(key); json_free(obj); return NULL; }
                JsonValue* val = parse_value(p); if (!val) { free(key); json_free(obj); return NULL; }
                json_set(obj, key, val); free(key);
            } while (adv(p) == ',');
        }
        if (adv(p) != '}') { json_free(obj); return NULL; }
        return obj;
    }
    return NULL;
}

JsonValue* json_parse(const char* input) {
    if (!input) return NULL;
    Parser p; memset(&p, 0, sizeof(p));
    p.data = input; p.len = strlen(input);
    return parse_value(&p);
}

static void json_free_children(JsonValue* v) {
    if (!v) return;
    if (v->str_val) { free(v->str_val); v->str_val = NULL; }
    if (v->children && v->size) {
        for (size_t i = 0; i < v->size; i++) {
            if (v->children[i].key) { free(v->children[i].key); v->children[i].key = NULL; }
            json_free_children(&v->children[i]);
        }
        free(v->children); v->children = NULL; v->size = 0;
    }
}

void json_free(JsonValue* root) {
    if (!root) return;
    json_free_children(root);
    free(root);
}

JsonType json_type(const JsonValue* v) { return v ? v->type : JSON_NULL; }
bool json_is_null(const JsonValue* v) { return v && v->type == JSON_NULL; }
bool json_is_bool(const JsonValue* v) { return v && v->type == JSON_BOOL; }
bool json_is_number(const JsonValue* v) { return v && v->type == JSON_NUMBER; }
bool json_is_string(const JsonValue* v) { return v && v->type == JSON_STRING; }
bool json_is_array(const JsonValue* v) { return v && v->type == JSON_ARRAY; }
bool json_is_object(const JsonValue* v) { return v && v->type == JSON_OBJECT; }
bool json_get_bool(const JsonValue* v) { return v && v->type == JSON_BOOL ? v->bool_val : 0; }
double json_get_number(const JsonValue* v) { return v && v->type == JSON_NUMBER ? v->num_val : 0; }
const char* json_get_string(const JsonValue* v) { return v && v->type == JSON_STRING ? v->str_val : NULL; }
size_t json_arr_size(const JsonValue* arr) { return arr && arr->type == JSON_ARRAY ? arr->size : 0; }
size_t json_obj_size(const JsonValue* obj) { return obj && obj->type == JSON_OBJECT ? obj->size : 0; }
const JsonValue* json_arr_at(const JsonValue* arr, size_t index) {
    if (!arr || arr->type != JSON_ARRAY || index >= arr->size) return NULL;
    return &arr->children[index];
}

