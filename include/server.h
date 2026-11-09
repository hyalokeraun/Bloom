#ifndef SERVER_H
#define SERVER_H

#include "http_parser.h"
#include "http_response.h"
#include "router.h"
#include "thread_pool.h"
#include "logger.h"
#include "config.h"

#include <winsock2.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Server configuration ───────────────────────────────────── */
typedef struct {
    const char* host;
    int         port;
    int         backlog;
    size_t      thread_pool_size;
    const char* log_file;
    LogLevel    log_level;
    size_t      max_body_size;      /* max request body bytes (default 4MB) */
    int         keep_alive_timeout; /* seconds, 0 = no keep-alive */
    bool        enable_cors;
} ServerConfig;

#define SERVER_DEFAULT_CONFIG { \
    "0.0.0.0", \
    8080, \
    128, \
    8, \
    "server.log", \
    LOG_INFO, \
    4 * 1024 * 1024, \
    65, \
    true \
}

/* ── Server handle ──────────────────────────────────────────── */
typedef struct {
    ServerConfig   config;
    Logger*        logger;
    Config*        settings;
    Router*        router;
    ThreadPool*    pool;
    SOCKET         listen_socket;
    CRITICAL_SECTION lock;
    HANDLE         shutdown_event;
    volatile long  running;     /* atomic flag */
    ULONGLONG      start_time;  /* server start time in ms since epoch */
} Server;

/* ── Public API ─────────────────────────────────────────────── */
Server* server_create(const ServerConfig* cfg);
void    server_destroy(Server* self);

/* Register a route on the server */
void    server_register(Server* self, HttpMethod method,
                        const char* pattern, RouteHandler handler);

/* Start the server (blocking). Returns 0 on success. */
int     server_start(Server* self);

/* Gracefully stop the server */
void    server_stop(Server* self);

/* ── CORS middleware helper ─────────────────────────────────── */
/* Call this on an HttpResponse to set CORS headers when enabled */
void server_apply_cors(HttpResponse* res);

/* Get server uptime in milliseconds */
ULONGLONG server_uptime_ms(const Server* self);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_H */
