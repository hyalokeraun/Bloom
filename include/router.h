#ifndef ROUTER_H
#define ROUTER_H

#include "http_parser.h"
#include "http_response.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Handler signature ──────────────────────────────────────── */
/* Returns a response owned by caller, or NULL for auto 404 */
typedef HttpResponse* (*RouteHandler)(const HttpRequest* req);

/* ── Route ──────────────────────────────────────────────────── */
typedef struct Route Route;
struct Route {
    HttpMethod   method;
    char         pattern[512];      /* e.g. "/api/users/:id" */
    RouteHandler handler;
    Route*       next;
};

/* ── Router ─────────────────────────────────────────────────── */
typedef struct Router Router;

Router* router_create(void);
void    router_destroy(Router* self);

/* Register a route */
void    router_register(Router* self, HttpMethod method,
                        const char* pattern, RouteHandler handler);

/* Match and dispatch */
HttpResponse* router_dispatch(Router* self, const HttpRequest* req);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_H */
