#define _CRT_SECURE_NO_WARNINGS
#include "http_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static char* trim(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char* end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) *end-- = '\0';
    return s;
}

const char* http_method_str(HttpMethod method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH: return "PATCH";
        case HTTP_HEAD: return "HEAD";
        case HTTP_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

HttpMethod http_method_from_str(const char* s) {
    if (!s) return HTTP_UNKNOWN;
    if (_stricmp(s, "GET") == 0) return HTTP_GET;
    if (_stricmp(s, "POST") == 0) return HTTP_POST;
    if (_stricmp(s, "PUT") == 0) return HTTP_PUT;
    if (_stricmp(s, "DELETE") == 0) return HTTP_DELETE;
    if (_stricmp(s, "PATCH") == 0) return HTTP_PATCH;
    if (_stricmp(s, "HEAD") == 0) return HTTP_HEAD;
    if (_stricmp(s, "OPTIONS") == 0) return HTTP_OPTIONS;
    return HTTP_UNKNOWN;
}

static void url_decode(char* dest, const char* src) {
    char* d = dest;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *d++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *d++ = ' ';
            src++;
        } else {
            *d++ = *src++;
        }
    }
    *d = '\0';
}

HttpRequest* http_request_parse(const char* raw, size_t len) {
    HttpRequest* req = (HttpRequest*)calloc(1, sizeof(HttpRequest));
    if (!req) return NULL;

    char* buf = (char*)malloc(len + 1);
    memcpy(buf, raw, len);
    buf[len] = '\0';

    char* p = buf;
    /* Parse request line */
    char* line_end = strchr(p, '\n');
    if (!line_end) { free(buf); free(req); return NULL; }
    *line_end = '\0';

    char* method_str = trim(p);
    /* Find space to split method from the rest of the request line */
    char* rest = strchr(method_str, ' ');
    if (!rest) { free(buf); free(req); return NULL; }
    *rest = '\0';
    req->method = http_method_from_str(method_str);
    strncpy_s(req->method_str, sizeof(req->method_str), method_str, _TRUNCATE);

    char* path_q = trim(rest + 1);
    char* http_ver = strrchr(path_q, ' ');
    if (http_ver) {
        *http_ver = '\0';
        strncpy_s(req->http_version, sizeof(req->http_version), http_ver + 1, _TRUNCATE);
    } else {
        strcpy_s(req->http_version, sizeof(req->http_version), "HTTP/1.1");
    }

    /* Split path and query */
    char* qmark = strchr(path_q, '?');
    if (qmark) {
        *qmark = '\0';
        strncpy_s(req->query, sizeof(req->query), qmark + 1, _TRUNCATE);
        url_decode(req->query, req->query);
    }
    strncpy_s(req->path, sizeof(req->path), path_q, _TRUNCATE);
    url_decode(req->path, req->path);

    /* Parse headers */
    p = line_end + 1;
    while (*p) {
        char* hdr_end = strchr(p, '\n');
        if (!hdr_end) break;
        *hdr_end = '\0';
        char* hdr = trim(p);
        if (hdr[0] == '\0') {
            /* End of headers, process body */
            p = hdr_end + 1;
            break;
        }
        char* colon = strchr(hdr, ':');
        if (colon) {
            *colon = '\0';
            char* name = trim(hdr);
            char* val = trim(colon + 1);
            if (req->header_count < 64) {
                strncpy_s(req->header_names[req->header_count], sizeof(req->header_names[0]), name, _TRUNCATE);
                strncpy_s(req->header_vals[req->header_count], sizeof(req->header_vals[0]), val, _TRUNCATE);
                req->header_count++;
            }
        }
        p = hdr_end + 1;
    }

    /* Parse body */
    if (*p) {
        const char* cl = http_request_get_header(req, "Content-Length");
        if (cl) {
            req->body_len = (size_t)strtoul(cl, NULL, 10);
            if (req->body_len > 0 && len - (size_t)(p - buf) >= req->body_len) {
                req->body = (char*)malloc(req->body_len + 1);
                memcpy(req->body, p, req->body_len);
                req->body[req->body_len] = '\0';
            }
        }
    }

    free(buf);
    return req;
}

void http_request_free(HttpRequest* self) {
    if (!self) return;
    free(self->body);
    free(self);
}

const char* http_request_get_header(const HttpRequest* req, const char* name) {
    if (!req || !name) return NULL;
    for (size_t i = 0; i < req->header_count; i++) {
        if (_stricmp(req->header_names[i], name) == 0)
            return req->header_vals[i];
    }
    return NULL;
}

bool http_request_has_body(const HttpRequest* req) {
    return req && req->body_len > 0 && req->body != NULL;
}
