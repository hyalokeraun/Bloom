#define _CRT_SECURE_NO_WARNINGS
#include "server.h"
#include "handlers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>


void server_apply_cors(HttpResponse* res) {
    if (!res) return;
    http_response_set_header(res, "Access-Control-Allow-Origin", "*");
    http_response_set_header(res, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, PATCH");
    http_response_set_header(res, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    http_response_set_header(res, "Access-Control-Max-Age", "86400");
}

Server* server_create(const ServerConfig* cfg) {
    Server* srv = (Server*)calloc(1, sizeof(Server));
    memcpy(&srv->config, cfg, sizeof(ServerConfig));
    srv->logger = logger_create(cfg->log_file, cfg->log_level);
    srv->shutdown_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    InitializeCriticalSection(&srv->lock);
    srv->running = 0;
    srv->settings = config_create();
    if (config_load(srv->settings, "config/server.ini") == 0) {
        const char* p = config_get(srv->settings, "port");
        if (p) srv->config.port = atoi(p);
        const char* h = config_get(srv->settings, "host");
        if (h) srv->config.host = h;
        const char* t = config_get(srv->settings, "threads");
        if (t) srv->config.thread_pool_size = (size_t)atoi(t);
    }
    srv->router = router_create();
    srv->pool = thread_pool_create(srv->config.thread_pool_size);
    srv->start_time = GetTickCount64();
    LOG(srv->logger, info, "Server initialized on %s:%d with %zu threads",
        srv->config.host, srv->config.port, srv->config.thread_pool_size);
    return srv;
}

void server_register(Server* self, HttpMethod method, const char* pattern, RouteHandler handler) {
    if (self && self->router) router_register(self->router, method, pattern, handler);
}

void server_destroy(Server* self) {
    if (!self) return;
    server_stop(self);
    thread_pool_destroy(self->pool);
    router_destroy(self->router);
    config_destroy(self->settings);
    logger_destroy(self->logger);
    CloseHandle(self->shutdown_event);
    DeleteCriticalSection(&self->lock);
    free(self);
}

/* ── Uptime helper ─────────────────────────────────────────── */
ULONGLONG server_uptime_ms(const Server* self) {
    if (!self) return 0;
    return GetTickCount64() - self->start_time;
}

/* ── CORS preflight handler ─────────────────────────────────── */
static HttpResponse* handle_cors_preflight(const HttpRequest* req) {
    (void)req;
    HttpResponse* res = http_response_new(HTTP_204_NO_CONTENT);
    server_apply_cors(res);
    return res;
}

/* ── Connection handler (runs in thread pool) ───────────────── */
static void handle_connection_thread(void* arg) {
    void** data = (void**)arg;
    Server* srv = (Server*)data[0];
    SOCKET client_socket = (SOCKET)(size_t)data[1];
    free(data);

    /* Track request start time for duration logging */
    DWORD t_start = (DWORD)GetTickCount64();

    /* Use heap-allocated buffer large enough for any HTTP request */
    size_t buf_capacity = 65536;
    char* buffer = (char*)malloc(buf_capacity);
    if (!buffer) {
        closesocket(client_socket);
        return;
    }

    int total_recv = 0;
    bool headers_complete = false;
    int received;

    while (!headers_complete && total_recv < 65535) {
        int remaining = (int)(buf_capacity - (size_t)total_recv - 1);
        if (remaining <= 0) break;
        received = (int)recv(client_socket, buffer + total_recv, remaining, 0);
        if (received <= 0) break;
        total_recv += received;
        buffer[total_recv] = '\0';
        if (total_recv >= 4 && memcmp(buffer + total_recv - 4, "\r\n\r\n", 4) == 0)
            headers_complete = true;
        if (!headers_complete && total_recv >= 2 && memcmp(buffer + total_recv - 2, "\n\n", 2) == 0)
            headers_complete = true;
    }

    if (total_recv == 0) { free(buffer); closesocket(client_socket); return; }

    /* Enforce max body size */
    if ((size_t)total_recv > srv->config.max_body_size) {
        HttpRequest* req = http_request_parse(buffer, (size_t)total_recv);
        HttpResponse* res = http_response_new(HTTP_413_ENTITY_TOO_LARGE);
        http_response_set_text_body(res, "Request body too large");
        char* s = http_response_to_string(res);
        if (s) { send(client_socket, s, (int)strlen(s), 0); free(s); }
        http_response_free(res);
        http_request_free(req);
        free(buffer);
        closesocket(client_socket);
        return;
    }

    HttpRequest* req = http_request_parse(buffer, (size_t)total_recv);
    if (!req) {
        HttpResponse* res = http_response_new(HTTP_400_BAD_REQUEST);
        http_response_set_text_body(res, "Bad Request");
        char* s = http_response_to_string(res);
        if (s) { send(client_socket, s, (int)strlen(s), 0); free(s); }
        http_response_free(res);
        free(buffer);
        closesocket(client_socket);
        return;
    }

    LOG(srv->logger, info, "%s %s", http_method_str(req->method), req->path);

    /* Handle CORS preflight for OPTIONS requests */
    if (req->method == HTTP_OPTIONS && srv->config.enable_cors) {
        HttpResponse* res = handle_cors_preflight(req);
        char* response_str = http_response_to_string(res);
        if (response_str) {
            int total_sent = 0, len = (int)strlen(response_str);
            while (total_sent < len) {
                int sent = (int)send(client_socket, response_str + total_sent, len - total_sent, 0);
                if (sent <= 0) break;
                total_sent += sent;
            }
            free(response_str);
        }
        http_response_free(res);
        http_request_free(req);
        free(buffer);
        closesocket(client_socket);
        DWORD elapsed_ms = (DWORD)GetTickCount64() - t_start;
        LOG(srv->logger, info, "Response sent: 204 (%u ms)", elapsed_ms);
        return;
    }

    /* Dispatch and send response, with SEH to catch handler crashes */
    __try {
        HttpResponse* res = router_dispatch(srv->router, req);
        if (!res) {
            res = http_response_new(HTTP_404_NOT_FOUND);
            http_response_set_text_body(res, "404 Not Found");
        }
        char* response_str = http_response_to_string(res);
        int bytes_sent = 0;
        if (response_str) {
            int total_sent = 0;
            int len = (int)strlen(response_str);
            while (total_sent < len) {
                int sent = (int)send(client_socket, response_str + total_sent, len - total_sent, 0);
                if (sent <= 0) break;
                total_sent += sent;
            }
            bytes_sent = total_sent;
            free(response_str);
        }
        http_request_free(req);
        http_response_free(res);
        free(buffer);
        closesocket(client_socket);
        DWORD elapsed_ms = (DWORD)GetTickCount64() - t_start;
        LOG(srv->logger, info, "Response sent: %d bytes (%u ms)", bytes_sent, elapsed_ms);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        /* Handler crashed — send a basic error response */
        LOG(srv->logger, error, "Handler exception: 0x%x", GetExceptionCode());
        HttpResponse* res = http_response_new(HTTP_500_INTERNAL);
        http_response_set_text_body(res, "Internal Server Error");
        char* s = http_response_to_string(res);
        if (s) { send(client_socket, s, (int)strlen(s), 0); free(s); }
        http_response_free(res);
        http_request_free(req);
        free(buffer);
        closesocket(client_socket);
    }
}

int server_start(Server* srv) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return -1;
    }

    srv->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_socket == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
        return -1;
    }

    int opt = 1;
    setsockopt(srv->listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)srv->config.port);
    addr.sin_addr.s_addr = inet_addr(srv->config.host);

    if (bind(srv->listen_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Bind failed on port %d: %d\n", srv->config.port, WSAGetLastError());
        closesocket(srv->listen_socket);
        WSACleanup();
        return -1;
    }

    if (listen(srv->listen_socket, srv->config.backlog) == SOCKET_ERROR) {
        fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
        closesocket(srv->listen_socket);
        WSACleanup();
        return -1;
    }

    InterlockedExchange(&srv->running, 1);
    LOG(srv->logger, info, "Server started: http://%s:%d", srv->config.host, srv->config.port);

    /* Setup default routes */
    router_register(srv->router, HTTP_GET, "/", handler_root);
    router_register(srv->router, HTTP_GET, "/health", handler_health);
    router_register(srv->router, HTTP_POST, "/api/echo", handler_echo);
    router_register(srv->router, HTTP_GET, "/api/users", handler_list_users);
    router_register(srv->router, HTTP_GET, "/api/users/:id", handler_get_user);
    router_register(srv->router, HTTP_POST, "/api/users", handler_create_user);
    router_register(srv->router, HTTP_PUT, "/api/users/:id", handler_update_user);
    router_register(srv->router, HTTP_PATCH, "/api/users/:id", handler_update_user);
    router_register(srv->router, HTTP_DELETE, "/api/users/:id", handler_delete_user);
    router_register(srv->router, HTTP_GET, "/api/env", handler_env);
    router_register(srv->router, HTTP_GET, "/api/server-info", handler_server_info);

    while (InterlockedCompareExchange(&srv->running, 1, 1)) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_socket = accept(srv->listen_socket, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket == INVALID_SOCKET) {
            if (!InterlockedCompareExchange(&srv->running, 1, 1)) break;
            LOG(srv->logger, error, "Accept failed: %d", WSAGetLastError());
            continue;
        }

        DWORD timeout = (DWORD)srv->config.keep_alive_timeout * 1000;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

        void** data = (void**)malloc(2 * sizeof(void*));
        data[0] = srv;
        data[1] = (void*)(size_t)client_socket;
        thread_pool_submit(srv->pool, handle_connection_thread, data);
    }

    closesocket(srv->listen_socket);
    WSACleanup();
    LOG(srv->logger, info, "Server stopped");
    return 0;
}

void server_stop(Server* srv) {
    if (!srv) return;
    InterlockedExchange(&srv->running, 0);
    if (srv->listen_socket != INVALID_SOCKET) {
        shutdown(srv->listen_socket, SD_BOTH);
    }
    SetEvent(srv->shutdown_event);
    LOG(srv->logger, info, "Server shutting down...");
}
