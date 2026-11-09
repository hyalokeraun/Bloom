#ifndef HANDLERS_H
#define HANDLERS_H

#include "http_parser.h"
#include "http_response.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Sample REST API handlers ───────────────────────────────── */

/* Health check — GET /health */
HttpResponse* handler_health(const HttpRequest* req);

/* Root — GET / */
HttpResponse* handler_root(const HttpRequest* req);

/* Echo — POST /api/echo */
HttpResponse* handler_echo(const HttpRequest* req);

/* Users collection — GET /api/users */
HttpResponse* handler_list_users(const HttpRequest* req);

/* Single user — GET /api/users/:id */
HttpResponse* handler_get_user(const HttpRequest* req);

/* Create user — POST /api/users */
HttpResponse* handler_create_user(const HttpRequest* req);

/* Update user — PUT /api/users/:id */
HttpResponse* handler_update_user(const HttpRequest* req);

/* Delete user — DELETE /api/users/:id */
HttpResponse* handler_delete_user(const HttpRequest* req);

/* Environment info — GET /api/env */
HttpResponse* handler_env(const HttpRequest* req);

/* Server info — GET /api/server-info */
HttpResponse* handler_server_info(const HttpRequest* req);

#ifdef __cplusplus
}
#endif

#endif /* HANDLERS_H */

#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Rate limiter configuration */
typedef struct {
    size_t max_requests;       /* max requests per window */
    double  window_seconds;    /* time window */
} RateLimiterConfig;

/* Token-bucket rate limiter */
typedef struct RateLimiter RateLimiter;

RateLimiter* rate_limiter_create(const RateLimiterConfig* cfg);
void         rate_limiter_destroy(RateLimiter* self);

/* Returns true if request is allowed, false if rate-limited */
bool rate_limiter_allow(RateLimiter* self, const char* client_ip);

/* Set the per-client rate limit count */
void rate_limiter_set_limit(RateLimiter* self, const char* client_ip, size_t limit);

#ifdef __cplusplus
}
#endif

#endif /* RATE_LIMITER_H */

#ifndef AUTH_MIDDLEWARE_H
#define AUTH_MIDDLEWARE_H

#include "http_parser.h"
#include "http_response.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Authentication middleware — checks Authorization header for Bearer token */
typedef struct {
    const char** allowed_tokens;  /* array of valid tokens, NULL-terminated */
    size_t       token_count;
} AuthConfig;

/* Create auth config with a single token */
AuthConfig auth_config_create(const char* token);

/* Create auth config with multiple tokens (NULL-terminated) */
AuthConfig auth_config_create_multi(const char** tokens);

/* Destroy auth config */
void auth_config_destroy(AuthConfig* cfg);

/* Authenticate a request — returns HttpResponse* on failure (owned by caller),
   or NULL if authentication succeeds */
HttpResponse* auth_middleware_check(const AuthConfig* cfg, const HttpRequest* req);

#ifdef __cplusplus
}
#endif

#endif /* AUTH_MIDDLEWARE_H */
