#define _CRT_SECURE_NO_WARNINGS
#include "router.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Router {
    Route* routes;
};

Router* router_create(void) {
    Router* r = (Router*)calloc(1, sizeof(Router));
    return r;
}

void router_destroy(Router* self) {
    if (!self) return;
    Route* r = self->routes;
    while (r) {
        Route* next = r->next;
        free(r);
        r = next;
    }
    free(self);
}

void router_register(Router* self, HttpMethod method, const char* pattern, RouteHandler handler) {
    if (!self || !pattern || !handler) return;
    Route* r = (Route*)calloc(1, sizeof(Route));
    r->method = method;
    strncpy_s(r->pattern, sizeof(r->pattern), pattern, _TRUNCATE);
    r->handler = handler;
    r->next = self->routes;
    self->routes = r;
}

static int split_path(const char* path, char** parts, size_t max_parts) {
    char* tmp = _strdup(path);
    int count = 0;
    char* saveptr;
    char* token = strtok_s(tmp, "/", &saveptr);
    while (token && count < (int)max_parts) {
        parts[count++] = _strdup(token);
        token = strtok_s(NULL, "/", &saveptr);
    }
    free(tmp);
    return count;
}

static int path_match(const char* pattern, const char* path) {
    char* pparts[64];
    char* aparts[64];
    int pc = split_path(pattern, pparts, 64);
    int ac = split_path(path, aparts, 64);
    int matched = (pc == ac) ? 1 : 0;
    if (matched) {
        for (int i = 0; i < pc && matched; i++) {
            if (pparts[i][0] != ':' && _stricmp(pparts[i], aparts[i]) != 0)
                matched = 0;
        }
    }
    for (int i = 0; i < pc; i++) free(pparts[i]);
    for (int i = 0; i < ac; i++) free(aparts[i]);
    return matched;
}

HttpResponse* router_dispatch(Router* self, const HttpRequest* req) {
    if (!self || !req) {
        HttpResponse* res = http_response_new(HTTP_404_NOT_FOUND);
        http_response_set_text_body(res, "404 Not Found");
        return res;
    }
    Route* r = self->routes;
    while (r) {
        if (r->method == req->method && path_match(r->pattern, req->path))
            return r->handler(req);
        r = r->next;
    }
    HttpResponse* res = http_response_new(HTTP_404_NOT_FOUND);
    char msg[256];
    snprintf(msg, sizeof(msg), "404 Not Found: %s %s", http_method_str(req->method), req->path);
    http_response_set_text_body(res, msg);
    return res;
}
