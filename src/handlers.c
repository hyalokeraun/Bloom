#define _CRT_SECURE_NO_WARNINGS
#include "handlers.h"
#include "json.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Rate Limiter */
typedef struct {
    char   ip[64];
    time_t window_start;
    size_t request_count;
    size_t max_requests;
    double window_seconds;
} RateLimiterClient;

struct RateLimiter {
    RateLimiterClient* clients;
    size_t count;
    size_t capacity;
    double default_window_seconds;
    size_t default_max_requests;
    CRITICAL_SECTION lock;
};

RateLimiter* rate_limiter_create(const RateLimiterConfig* cfg) {
    RateLimiter* rl = (RateLimiter*)calloc(1, sizeof(RateLimiter));
    rl->capacity = 64;
    rl->clients = (RateLimiterClient*)calloc(rl->capacity, sizeof(RateLimiterClient));
    rl->default_window_seconds = cfg->window_seconds > 0 ? cfg->window_seconds : 60.0;
    rl->default_max_requests = cfg->max_requests > 0 ? cfg->max_requests : 100;
    InitializeCriticalSection(&rl->lock);
    return rl;
}
void rate_limiter_destroy(RateLimiter* self) {
    if (!self) return;
    DeleteCriticalSection(&self->lock);
    free(self->clients);
    free(self);
}
static RateLimiterClient* find_or_create_client(RateLimiter* rl, const char* ip) {
    for (size_t i = 0; i < rl->count; i++) {
        if (strcmp(rl->clients[i].ip, ip) == 0) return &rl->clients[i];
    }
    if (rl->count >= rl->capacity) rl->count = rl->count / 2;
    RateLimiterClient* c = &rl->clients[rl->count++];
    strncpy_s(c->ip, sizeof(c->ip), ip, _TRUNCATE);
    c->window_start = time(NULL);
    c->request_count = 0;
    c->max_requests = rl->default_max_requests;
    c->window_seconds = rl->default_window_seconds;
    return c;
}
bool rate_limiter_allow(RateLimiter* self, const char* client_ip) {
    if (!self || !client_ip) return true;
    EnterCriticalSection(&self->lock);
    RateLimiterClient* c = find_or_create_client(self, client_ip);
    time_t now = time(NULL);
    if (difftime(now, c->window_start) >= c->window_seconds) {
        c->window_start = now;
        c->request_count = 1;
    } else {
        c->request_count++;
    }
    bool allowed = c->request_count <= c->max_requests;
    LeaveCriticalSection(&self->lock);
    return allowed;
}

/* Auth Middleware */
AuthConfig auth_config_create(const char* token) {
    AuthConfig cfg;
    cfg.token_count = 1;
    cfg.allowed_tokens = (const char**)calloc(2, sizeof(const char*));
    cfg.allowed_tokens[0] = token;
    cfg.allowed_tokens[1] = NULL;
    return cfg;
}
AuthConfig auth_config_create_multi(const char** tokens) {
    AuthConfig cfg;
    size_t count = 0;
    while (tokens && tokens[count]) count++;
    cfg.token_count = count;
    cfg.allowed_tokens = (const char**)calloc(count + 1, sizeof(const char*));
    for (size_t i = 0; i < count; i++) cfg.allowed_tokens[i] = tokens[i];
    cfg.allowed_tokens[count] = NULL;
    return cfg;
}
void auth_config_destroy(AuthConfig* cfg) {
    if (cfg) { free((void*)cfg->allowed_tokens); cfg->allowed_tokens = NULL; cfg->token_count = 0; }
}
HttpResponse* auth_middleware_check(const AuthConfig* cfg, const HttpRequest* req) {
    if (!cfg || !req) return NULL;
    const char* auth_header = http_request_get_header(req, "Authorization");
    if (!auth_header) {
        JsonValue* root = json_obj_val();
        json_set(root, "error", json_str_val("Unauthorized"));
        json_set(root, "message", json_str_val("Missing Authorization header"));
        json_set(root, "hint", json_str_val("Use: Authorization: Bearer <token>"));
        char* json = json_to_string(root, true); json_free(root);
        HttpResponse* res = http_response_new(HTTP_401_UNAUTHORIZED);
        http_response_set_json_body(res, json); free(json);
        return res;
    }
    const char* token = NULL;
    if (_strnicmp(auth_header, "Bearer ", 7) == 0) {
        token = auth_header + 7;
        while (token[0] == ' ') token++;
    } else if (_strnicmp(auth_header, "Bearer=", 7) == 0) {
        token = auth_header + 7;
        while (token[0] == ' ') token++;
    }
    if (!token || token[0] == '\0') {
        JsonValue* root = json_obj_val();
        json_set(root, "error", json_str_val("Unauthorized"));
        json_set(root, "message", json_str_val("Invalid Authorization format"));
        char* json = json_to_string(root, true); json_free(root);
        HttpResponse* res = http_response_new(HTTP_401_UNAUTHORIZED);
        http_response_set_json_body(res, json); free(json);
        return res;
    }
    for (size_t i = 0; i < cfg->token_count; i++) {
        if (cfg->allowed_tokens[i] && strcmp(token, cfg->allowed_tokens[i]) == 0) return NULL;
    }
    JsonValue* root = json_obj_val();
    json_set(root, "error", json_str_val("Forbidden"));
    json_set(root, "message", json_str_val("Invalid or expired token"));
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_403_FORBIDDEN);
    http_response_set_json_body(res, json); free(json);
    return res;
}

/* User Store (thread-safe) */
typedef struct { int id; char name[128]; char email[128]; bool active; } User;
static User g_users[1024];
static size_t g_user_count = 0;
static int g_next_id = 1;
static CRITICAL_SECTION g_users_lock;
static void init_users(void) {
    if (g_user_count > 0) return;
    const char* names[] = {"Alice","Bob","Charlie","Diana","Eve"};
    const char* emails[] = {"alice@test.com","bob@test.com","charlie@test.com","diana@test.com","eve@test.com"};
    for (int i = 0; i < 5; i++) {
        g_users[g_user_count].id = (int)g_user_count + 1;
        strncpy_s(g_users[g_user_count].name, sizeof(g_users[g_user_count].name), names[i], _TRUNCATE);
        strncpy_s(g_users[g_user_count].email, sizeof(g_users[g_user_count].email), emails[i], _TRUNCATE);
        g_users[g_user_count].active = true;
        g_user_count++;
    }
    InitializeCriticalSection(&g_users_lock);
}
static User* find_user_by_id(int id) {
    for (size_t i = 0; i < g_user_count; i++)
        if (g_users[i].id == id) return &g_users[i];
    return NULL;
}
static char* extract_id_param(const HttpRequest* req) {
    const char* slash = strrchr(req->path, '/');
    if (!slash) return NULL;
    char* id_str = strdup(slash + 1);
    for (char* c = id_str; *c; c++)
        if (*c < '0' || *c > '9') { free(id_str); return NULL; }
    return id_str;
}

HttpResponse* handler_root(const HttpRequest* req) {
    (void)req;
    JsonValue* root = json_obj_val();
    json_set(root, "message", json_str_val("Welcome to C Backend Server"));
    json_set(root, "version", json_str_val("1.0.0"));
    JsonValue* ep = json_arr_val();
    json_append(ep, json_str_val("/health"));
    json_append(ep, json_str_val("/api/users"));
    json_append(ep, json_str_val("/api/echo"));
    json_set(root, "endpoints", ep);
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_health(const HttpRequest* req) {
    (void)req;
    JsonValue* root = json_obj_val();
    time_t now = time(NULL);
    char ts[64]; struct tm tm_info;
    localtime_s(&tm_info, &now);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tm_info);
    json_set(root, "status", json_str_val("healthy"));
    json_set(root, "timestamp", json_str_val(ts));
    json_set(root, "uptime_ms", json_num_val((double)GetTickCount64()));
    char* json = json_to_string(root, false); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_echo(const HttpRequest* req) {
    JsonValue* root = json_obj_val();
    json_set(root, "method", json_str_val(http_method_str(req->method)));
    json_set(root, "path", json_str_val(req->path));
    json_set(root, "query", json_str_val(req->query[0] ? req->query : "(none)"));
    if (http_request_has_body(req)) {
        JsonValue* parsed = json_parse(req->body);
        if (parsed) {
            json_set(root, "parsed_body", parsed);
            json_set(root, "body_raw", json_str_val(req->body));
            json_free(parsed);
        } else {
            json_set(root, "body_raw", json_str_val(req->body));
            json_set(root, "parse_error", json_str_val("Failed to parse JSON body"));
        }
        json_set(root, "body_length", json_num_val((double)req->body_len));
    }
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_list_users(const HttpRequest* req) {
    (void)req;
    init_users();
    JsonValue* root = json_obj_val();
    JsonValue* arr = json_arr_val();
    EnterCriticalSection(&g_users_lock);
    for (size_t i = 0; i < g_user_count; i++) {
        if (!g_users[i].active) continue;
        JsonValue* u = json_obj_val();
        json_set(u, "id", json_num_val((double)g_users[i].id));
        json_set(u, "name", json_str_val(g_users[i].name));
        json_set(u, "email", json_str_val(g_users[i].email));
        json_set(u, "active", json_bool_val(g_users[i].active));
        json_append(arr, u);
    }
    LeaveCriticalSection(&g_users_lock);
    json_set(root, "users", arr);
    json_set(root, "total", json_num_val((double)arr->size));
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_get_user(const HttpRequest* req) {
    init_users();
    char* id_s = extract_id_param(req);
    if (!id_s) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "Invalid path"); return r; }
    int id = atoi(id_s); free(id_s);
    User* u = find_user_by_id(id);
    if (!u) { HttpResponse* r = http_response_new(HTTP_404_NOT_FOUND); http_response_set_text_body(r, "User not found"); return r; }
    EnterCriticalSection(&g_users_lock);
    JsonValue* root = json_obj_val();
    json_set(root, "id", json_num_val((double)u->id));
    json_set(root, "name", json_str_val(u->name));
    json_set(root, "email", json_str_val(u->email));
    json_set(root, "active", json_bool_val(u->active));
    LeaveCriticalSection(&g_users_lock);
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_create_user(const HttpRequest* req) {
    init_users();
    if (!http_request_has_body(req)) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "JSON body required"); return r; }
    JsonValue* body = json_parse(req->body);
    if (!body) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "Invalid JSON"); return r; }
    const char* name = json_get_string(json_get(body, "name"));
    const char* email = json_get_string(json_get(body, "email"));
    if (!name || !email) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "name and email required"); json_free(body); return r; }
    EnterCriticalSection(&g_users_lock);
    g_users[g_user_count].id = g_next_id++;
    strncpy_s(g_users[g_user_count].name, sizeof(g_users[g_user_count].name), name, _TRUNCATE);
    strncpy_s(g_users[g_user_count].email, sizeof(g_users[g_user_count].email), email, _TRUNCATE);
    g_users[g_user_count].active = true;
    g_user_count++;
    User* u = &g_users[g_user_count - 1];
    LeaveCriticalSection(&g_users_lock);
    json_free(body);
    JsonValue* root = json_obj_val();
    json_set(root, "id", json_num_val((double)u->id));
    json_set(root, "name", json_str_val(u->name));
    json_set(root, "email", json_str_val(u->email));
    json_set(root, "message", json_str_val("Created"));
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_201_CREATED);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_update_user(const HttpRequest* req) {
    init_users();
    char* id_s = extract_id_param(req);
    if (!id_s) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "Invalid path"); return r; }
    int id = atoi(id_s); free(id_s);
    User* u = find_user_by_id(id);
    if (!u) { HttpResponse* r = http_response_new(HTTP_404_NOT_FOUND); http_response_set_text_body(r, "User not found"); return r; }
    if (!http_request_has_body(req)) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "JSON body required"); return r; }
    JsonValue* body = json_parse(req->body);
    if (!body) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "Invalid JSON"); return r; }
    const char* name = json_get_string(json_get(body, "name"));
    const char* email = json_get_string(json_get(body, "email"));
    if (name) { EnterCriticalSection(&g_users_lock); strncpy_s(u->name, sizeof(u->name), name, _TRUNCATE); LeaveCriticalSection(&g_users_lock); }
    if (email) { EnterCriticalSection(&g_users_lock); strncpy_s(u->email, sizeof(u->email), email, _TRUNCATE); LeaveCriticalSection(&g_users_lock); }
    json_free(body);
    JsonValue* root = json_obj_val();
    json_set(root, "id", json_num_val((double)u->id));
    json_set(root, "name", json_str_val(u->name));
    json_set(root, "email", json_str_val(u->email));
    json_set(root, "message", json_str_val("Updated"));
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_delete_user(const HttpRequest* req) {
    init_users();
    char* id_s = extract_id_param(req);
    if (!id_s) { HttpResponse* r = http_response_new(HTTP_400_BAD_REQUEST); http_response_set_text_body(r, "Invalid path"); return r; }
    int id = atoi(id_s); free(id_s);
    User* u = find_user_by_id(id);
    if (!u) { HttpResponse* r = http_response_new(HTTP_404_NOT_FOUND); http_response_set_text_body(r, "User not found"); return r; }
    EnterCriticalSection(&g_users_lock);
    u->active = false;
    JsonValue* root = json_obj_val();
    json_set(root, "id", json_num_val((double)u->id));
    json_set(root, "name", json_str_val(u->name));
    json_set(root, "message", json_str_val("Deleted"));
    LeaveCriticalSection(&g_users_lock);
    char* json = json_to_string(root, false); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_env(const HttpRequest* req) {
    JsonValue* root = json_obj_val();
    JsonValue* hdr = json_obj_val();
    for (size_t i = 0; i < req->header_count; i++)
        json_set(hdr, req->header_names[i], json_str_val(req->header_vals[i]));
    json_set(root, "headers", hdr);
    json_set(root, "method", json_str_val(http_method_str(req->method)));
    json_set(root, "path", json_str_val(req->path));
    json_set(root, "query", json_str_val(req->query[0] ? req->query : "(none)"));
    json_set(root, "http_version", json_str_val(req->http_version));
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}

HttpResponse* handler_server_info(const HttpRequest* req) {
    (void)req;
    JsonValue* root = json_obj_val();
    json_set(root, "server", json_str_val("C Backend Server"));
    json_set(root, "version", json_str_val("1.0.0"));
    json_set(root, "platform", json_str_val("Windows"));
    json_set(root, "compiler", json_str_val("MSVC"));
    SYSTEM_INFO si; GetSystemInfo(&si);
    char cpu_buf[64]; snprintf(cpu_buf, sizeof(cpu_buf), "%u", si.dwNumberOfProcessors);
    json_set(root, "processors", json_str_val(cpu_buf));
    MEMORYSTATUS mem; GlobalMemoryStatus(&mem);
    json_set(root, "memory_load_percent", json_num_val((double)mem.dwMemoryLoad));
    json_set(root, "endpoints", json_arr_val());
    JsonValue* ep = json_arr_val();
    json_append(ep, json_str_val("GET /"));
    json_append(ep, json_str_val("GET /health"));
    json_append(ep, json_str_val("POST /api/echo"));
    json_append(ep, json_str_val("GET /api/users"));
    json_append(ep, json_str_val("GET /api/users/:id"));
    json_append(ep, json_str_val("POST /api/users"));
    json_append(ep, json_str_val("PUT /api/users/:id"));
    json_append(ep, json_str_val("PATCH /api/users/:id"));
    json_append(ep, json_str_val("DELETE /api/users/:id"));
    json_append(ep, json_str_val("GET /api/env"));
    json_append(ep, json_str_val("GET /api/server-info"));
    json_set(root, "endpoints", ep);
    char* json = json_to_string(root, true); json_free(root);
    HttpResponse* res = http_response_new(HTTP_200_OK);
    http_response_set_json_body(res, json); free(json);
    return res;
}
