#define _CRT_SECURE_NO_WARNINGS
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

/* ── Internal state ─────────────────────────────────────────── */
struct Logger {
    FILE*       fp;
    LogLevel    min_level;
    char        file_path[512];
    bool        to_console;
    CRITICAL_SECTION mutex;
};

/* ── Global logger (singleton) ──────────────────────────────── */
static Logger* g_logger = NULL;

/* ── Timestamp helper ───────────────────────────────────────── */
static void timestamp(char* buf, size_t buflen) {
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm_info);
}

/* ── Log level name ─────────────────────────────────────────── */
static const char* level_name(LogLevel level) {
    switch (level) {
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO ";
        case LOG_WARN:    return "WARN ";
        case LOG_ERROR:   return "ERROR";
        case LOG_FATAL:   return "FATAL";
        default:          return "NONE ";
    }
}

/* ── Core logging ───────────────────────────────────────────── */
static void logger_vlog(Logger* self, LogLevel level, const char* fmt, va_list args) {
    if (level < self->min_level)
        return;

    EnterCriticalSection(&self->mutex);

    char ts[32];
    timestamp(ts, sizeof(ts));

    /* Build the full message */
    char msg[4096];
    int written = _vscprintf(fmt, args) + 256;
    if (written > (int)sizeof(msg) - 1)
        written = (int)sizeof(msg) - 1;

    va_list args_copy;
    va_copy(args_copy, args);
    /* We need to re-construct, but va_list can only be used once */
    /* Instead, just use vsnprintf */
    va_end(args_copy);

    int n = vsnprintf(msg, sizeof(msg), fmt, args);

    char line[4224];
    if (n < 0) n = 0;
    snprintf(line, sizeof(line), "[%s] [%s] %s\n", ts, level_name(level), msg);

    if (self->to_console) {
        /* Colorize for console */
        switch (level) {
            case LOG_DEBUG:   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7); break;
            case LOG_INFO:    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10); break;
            case LOG_WARN:    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14); break;
            case LOG_ERROR:   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12); break;
            case LOG_FATAL:   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15); break;
            default:          break;
        }
        fwrite(line, 1, strlen(line), stdout);
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    }

    if (self->fp) {
        fwrite(line, 1, strlen(line), self->fp);
        fflush(self->fp);
    }

    LeaveCriticalSection(&self->mutex);
}

/* ── Public API ─────────────────────────────────────────────── */
Logger* logger_create(const char* file_path, LogLevel min_level) {
    Logger* self = (Logger*)calloc(1, sizeof(Logger));
    if (!self) return NULL;

    self->min_level = min_level;
    self->to_console = true;

    if (file_path && file_path[0]) {
        strncpy_s(self->file_path, sizeof(self->file_path), file_path, _TRUNCATE);
        self->fp = fopen(file_path, "a");
        if (!self->fp) {
            fprintf(stderr, "Warning: cannot open log file '%s': %s\n",
                    file_path, strerror(errno));
        }
    }

    InitializeCriticalSection(&self->mutex);
    return self;
}

void logger_destroy(Logger* self) {
    if (!self) return;
    if (self->fp) {
        fflush(self->fp);
        fclose(self->fp);
    }
    DeleteCriticalSection(&self->mutex);
    free(self);
}

void logger_set_level(Logger* self, LogLevel level) {
    if (self) self->min_level = level;
}

void logger_set_file(Logger* self, const char* file_path) {
    if (!self || !file_path) return;

    if (self->fp) {
        fflush(self->fp);
        fclose(self->fp);
    }

    strncpy_s(self->file_path, sizeof(self->file_path), file_path, _TRUNCATE);
    self->fp = fopen(file_path, "a");
}

void logger_debug(Logger* self, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger_vlog(self, LOG_DEBUG, fmt, args);
    va_end(args);
}

void logger_info(Logger* self, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger_vlog(self, LOG_INFO, fmt, args);
    va_end(args);
}

void logger_warn(Logger* self, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger_vlog(self, LOG_WARN, fmt, args);
    va_end(args);
}

void logger_error(Logger* self, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger_vlog(self, LOG_ERROR, fmt, args);
    va_end(args);
}

void logger_fatal(Logger* self, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger_vlog(self, LOG_FATAL, fmt, args);
    va_end(args);
}

/* ── Global logger ──────────────────────────────────────────── */
void logger_global_init(const char* file_path, LogLevel min_level) {
    if (g_logger) return;
    g_logger = logger_create(file_path, min_level);
}

void logger_global_shutdown(void) {
    if (g_logger) {
        logger_destroy(g_logger);
        g_logger = NULL;
    }
}

Logger* logger_global_get(void) {
    return g_logger;
}
