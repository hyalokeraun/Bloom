#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── HTTP Methods ───────────────────────────────────────────── */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_HEAD,
    HTTP_OPTIONS,
    HTTP_UNKNOWN
} HttpMethod;

/* ── HTTP Request ───────────────────────────────────────────── */
typedef struct {
    HttpMethod method;
    char       method_str[8];
    char       path[2048];
    char       query[2048];       /* everything after '?' */
    char       http_version[16];

    size_t     header_count;
    char       header_names[64][64];
    char       header_vals[64][512];

    size_t     body_len;
    char*      body;              /* owned */
} HttpRequest;

/* ── Parsing ────────────────────────────────────────────────── */
HttpRequest* http_request_parse(const char* raw, size_t len);
void         http_request_free(HttpRequest* self);

const char*  http_method_str(HttpMethod method);
HttpMethod   http_method_from_str(const char* s);

/* Header lookup */
const char*  http_request_get_header(const HttpRequest* req, const char* name);
bool         http_request_has_body(const HttpRequest* req);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_PARSER_H */
