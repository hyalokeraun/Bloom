#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── HTTP Status Codes ──────────────────────────────────────── */
typedef enum {
    HTTP_200_OK           = 200,
    HTTP_201_CREATED      = 201,
    HTTP_204_NO_CONTENT   = 204,
    HTTP_301_MOVED        = 301,
    HTTP_304_NOT_MODIFIED = 304,
    HTTP_400_BAD_REQUEST  = 400,
    HTTP_401_UNAUTHORIZED = 401,
    HTTP_403_FORBIDDEN    = 403,
    HTTP_404_NOT_FOUND    = 404,
    HTTP_405_METHOD_NOT_ALLOWED = 405,
    HTTP_408_REQUEST_TIMEOUT  = 408,
    HTTP_413_ENTITY_TOO_LARGE = 413,
    HTTP_415_UNSUPPORTED_MEDIA_TYPE = 415,
    HTTP_429_TOO_MANY_REQUESTS    = 429,
    HTTP_500_INTERNAL       = 500,
    HTTP_502_BAD_GATEWAY    = 502,
    HTTP_503_SERVICE_UNAVAIL= 503,
    HTTP_504_GATEWAY_TIMEOUT= 504
} HttpStatus;

/* ── Response ───────────────────────────────────────────────── */
typedef struct {
    HttpStatus status;
    char*      status_text;   /* computed from status */

    size_t     header_count;
    char       header_names[32][64];
    char       header_vals[32][512];

    char*      body;          /* owned */
    size_t     body_len;

    bool       close_connection;
} HttpResponse;

/* ── Builders ───────────────────────────────────────────────── */
HttpResponse* http_response_new(HttpStatus status);

void          http_response_set_status_text(HttpResponse* res, const char* text);
void          http_response_set_header(HttpResponse* res, const char* name, const char* val);
void          http_response_set_body(HttpResponse* res, const char* body, size_t len);
void          http_response_set_json_body(HttpResponse* res, const char* json_str);
void          http_response_set_text_body(HttpResponse* res, const char* text);

void          http_response_close_connection(HttpResponse* res);

/* Serialise to HTTP response string */
char* http_response_to_string(const HttpResponse* res);

/* Free */
void http_response_free(HttpResponse* self);

/* ── Helpers ────────────────────────────────────────────────── */
const char* http_status_text(HttpStatus code);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_RESPONSE_H */
