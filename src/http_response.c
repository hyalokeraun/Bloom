#define _CRT_SECURE_NO_WARNINGS
#include "http_response.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* http_status_text(HttpStatus code) {
    switch (code) {
        case HTTP_200_OK: return "OK";
        case HTTP_201_CREATED: return "Created";
        case HTTP_204_NO_CONTENT: return "No Content";
        case HTTP_301_MOVED: return "Moved Permanently";
        case HTTP_304_NOT_MODIFIED: return "Not Modified";
        case HTTP_400_BAD_REQUEST: return "Bad Request";
        case HTTP_401_UNAUTHORIZED: return "Unauthorized";
        case HTTP_403_FORBIDDEN: return "Forbidden";
        case HTTP_404_NOT_FOUND: return "Not Found";
        case HTTP_405_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_408_REQUEST_TIMEOUT: return "Request Timeout";
        case HTTP_413_ENTITY_TOO_LARGE: return "Entity Too Large";
        case HTTP_415_UNSUPPORTED_MEDIA_TYPE: return "Unsupported Media Type";
        case HTTP_429_TOO_MANY_REQUESTS: return "Too Many Requests";
        case HTTP_500_INTERNAL: return "Internal Server Error";
        case HTTP_502_BAD_GATEWAY: return "Bad Gateway";
        case HTTP_503_SERVICE_UNAVAIL: return "Service Unavailable";
        case HTTP_504_GATEWAY_TIMEOUT: return "Gateway Timeout";
        default: return "Unknown";
    }
}

HttpResponse* http_response_new(HttpStatus status) {
    HttpResponse* res = (HttpResponse*)calloc(1, sizeof(HttpResponse));
    res->status = status;
    res->status_text = (char*)http_status_text(status);
    return res;
}

void http_response_set_status_text(HttpResponse* res, const char* text) {
    if (res) res->status_text = (char*)text;
}

void http_response_set_header(HttpResponse* res, const char* name, const char* val) {
    if (!res) return;
    if (res->header_count < 32) {
        strncpy_s(res->header_names[res->header_count], sizeof(res->header_names[0]), name, _TRUNCATE);
        strncpy_s(res->header_vals[res->header_count], sizeof(res->header_vals[0]), val, _TRUNCATE);
        res->header_count++;
    }
}

void http_response_set_body(HttpResponse* res, const char* body, size_t len) {
    if (!res) return;
    free(res->body);
    res->body = (char*)malloc(len + 1);
    memcpy(res->body, body, len);
    res->body[len] = '\0';
    res->body_len = len;
}

void http_response_set_json_body(HttpResponse* res, const char* json_str) {
    if (!res || !json_str) return;
    http_response_set_body(res, json_str, strlen(json_str));
    http_response_set_header(res, "Content-Type", "application/json");
}

void http_response_set_text_body(HttpResponse* res, const char* text) {
    if (!res || !text) return;
    http_response_set_body(res, text, strlen(text));
    http_response_set_header(res, "Content-Type", "text/plain; charset=utf-8");
}

void http_response_close_connection(HttpResponse* res) {
    if (res) res->close_connection = true;
}

char* http_response_to_string(const HttpResponse* res) {
    if (!res) return NULL;
    size_t buf_size = 4096;
    char* buf = (char*)malloc(buf_size);
    if (!buf) return NULL;
    size_t pos = 0;

    /* Status line */
    int n = snprintf(buf, buf_size, "HTTP/1.1 %d %s\r\n", res->status, res->status_text);
    if (n < 0 || (size_t)n >= buf_size) { free(buf); return NULL; }
    pos = (size_t)n;

    /* Connection header */
    n = snprintf(buf + pos, buf_size - pos, "Connection: %s\r\n",
        res->close_connection ? "close" : "keep-alive");
    if (n < 0 || (size_t)n >= buf_size - pos) { free(buf); return NULL; }
    pos += (size_t)n;

    /* Add Content-Length if body exists */
    if (res->body && res->body_len > 0) {
        /* Check if Content-Type already set */
        bool has_ct = false;
        for (size_t i = 0; i < res->header_count; i++)
            if (_stricmp(res->header_names[i], "Content-Type") == 0) { has_ct = true; break; }
        if (!has_ct) {
            n = snprintf(buf + pos, buf_size - pos, "Content-Type: text/html\r\n");
            if (n > 0) pos += (size_t)n;
        }
        n = snprintf(buf + pos, buf_size - pos, "Content-Length: %zu\r\n", res->body_len);
        if (n > 0) pos += (size_t)n;
    }

    /* Custom headers */
    for (size_t i = 0; i < res->header_count && pos < buf_size - 1; i++) {
        n = snprintf(buf + pos, buf_size - pos, "%s: %s\r\n", res->header_names[i], res->header_vals[i]);
        if (n > 0) pos += (size_t)n;
    }

    /* Blank line */
    if (pos < buf_size - 2) { buf[pos] = '\r'; buf[pos + 1] = '\n'; pos += 2; }

    /* Body */
    if (res->body && res->body_len > 0 && pos + res->body_len < buf_size) {
        memcpy(buf + pos, res->body, res->body_len);
        pos += res->body_len;
    }

    buf[pos] = '\0';
    /* Return a copy so caller can free it independently */
    char* result = (char*)malloc(pos + 1);
    if (result) { memcpy(result, buf, pos + 1); }
    free(buf);
    return result;
}

void http_response_free(HttpResponse* self) {
    if (!self) return;
    free(self->body);
    free(self);
}
