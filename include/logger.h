#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Log Levels ─────────────────────────────────────────────── */
typedef enum {
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARN    = 2,
    LOG_ERROR   = 3,
    LOG_FATAL   = 4,
    LOG_NONE    = 5
} LogLevel;

/* ── Logger handle ──────────────────────────────────────────── */
typedef struct Logger Logger;

/* ── Public API ─────────────────────────────────────────────── */
Logger* logger_create(const char* file_path, LogLevel min_level);
void    logger_destroy(Logger* self);

void    logger_set_level(Logger* self, LogLevel level);
void    logger_set_file(Logger* self, const char* file_path);

void    logger_debug(Logger* self, const char* fmt, ...);
void    logger_info(Logger* self, const char* fmt, ...);
void    logger_warn(Logger* self, const char* fmt, ...);
void    logger_error(Logger* self, const char* fmt, ...);
void    logger_fatal(Logger* self, const char* fmt, ...);

/* Convenience macro: auto-fills __FILE__ and __LINE__ */
#define LOG(logger, level, ...) \
    do { \
        if ((logger)) logger_##level((logger), "[%s:%d] ", __FILE__, __LINE__); \
        if ((logger)) logger_##level((logger), __VA_ARGS__); \
    } while (0)

/* Thread-safe global logger (singleton) */
void    logger_global_init(const char* file_path, LogLevel min_level);
void    logger_global_shutdown(void);
Logger* logger_global_get(void);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
