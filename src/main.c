#define _CRT_SECURE_NO_WARNINGS
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* Global for Ctrl+C handler */
static Server* g_ctrl_srv = NULL;

static int ctrl_handler(DWORD ctrl) {
    if (ctrl == CTRL_C_EVENT || ctrl == CTRL_CLOSE_EVENT) {
        if (g_ctrl_srv) server_stop(g_ctrl_srv);
        return TRUE;
    }
    return FALSE;
}

static void register_builtin_routes(Server* srv) {
    (void)srv;
    /* Routes are registered inside server_start() by default.
     * This function exists for manual registration if desired. */
}

int main(int argc, char* argv[]) {
    printf("============================================\n");
    printf("   C Backend Server v1.0.0\n");
    printf("   Windows / MSVC / Winsock2\n");
    printf("============================================\n\n");

    ServerConfig cfg = SERVER_DEFAULT_CONFIG;
    Server* srv = NULL;

    /* Parse command-line args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            cfg.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            cfg.host = argv[++i];
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            cfg.thread_pool_size = (size_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            cfg.log_file = argv[++i];
        } else if (strcmp(argv[i], "--debug") == 0) {
            cfg.log_level = LOG_DEBUG;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("  --port <n>       Port number (default: 8080)\n");
            printf("  --host <addr>    Bind address (default: 0.0.0.0)\n");
            printf("  --threads <n>    Thread pool size (default: 8)\n");
            printf("  --log <file>     Log file path (default: server.log)\n");
            printf("  --debug          Enable debug logging\n");
            printf("  -h, --help       Show this help\n");
            return 0;
        }
    }

    /* Create server */
    srv = server_create(&cfg);
    if (!srv) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }

    /* Register any custom routes here */
    register_builtin_routes(srv);

    printf("Starting server on http://%s:%d\n", cfg.host, cfg.port);
    printf("Thread pool: %zu workers\n", cfg.thread_pool_size);
    printf("Press Ctrl+C to stop.\n\n");

    /* Console control handler for graceful shutdown */
    g_ctrl_srv = srv;
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, TRUE);

    int result = server_start(srv);
    server_destroy(srv);

    printf("\nServer exited with code %d\n", result);
    return result;
}
