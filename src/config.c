#define _CRT_SECURE_NO_WARNINGS
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

struct Config {
    char** keys;
    char** vals;
    size_t count;
    size_t cap;
};

Config* config_create(void) {
    Config* c = (Config*)calloc(1, sizeof(Config));
    c->cap = 16;
    c->keys = (char**)calloc(c->cap, sizeof(char*));
    c->vals = (char**)calloc(c->cap, sizeof(char*));
    return c;
}

void config_destroy(Config* self) {
    if (!self) return;
    for (size_t i = 0; i < self->count; i++) {
        free(self->keys[i]);
        free(self->vals[i]);
    }
    free(self->keys);
    free(self->vals);
    free(self);
}

int config_load(Config* self, const char* file_path) {
    FILE* fp = fopen(file_path, "r");
    if (!fp) { fprintf(stderr, "Cannot open config: %s\n", file_path); return -1; }
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        /* Trim trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        /* Skip blanks and comments */
        if (len == 0 || line[0] == '#' || line[0] == ';') continue;
        /* Find '=' */
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        /* Trim leading/trailing whitespace from key */
        char* k = line;
        while (*k == ' ' || *k == '\t') k++;
        char* end = k + strlen(k) - 1;
        while (end > k && (*end == ' ' || *end == '\t')) *end-- = '\0';
        char* v = eq + 1;
        while (*v == ' ' || *v == '\t') v++;
        end = v + strlen(v) - 1;
        while (end > v && (*end == ' ' || *end == '\t')) *end-- = '\0';
        /* Remove surrounding quotes from value */
        if (*v == '"' || *v == '\'') {
            v++;
            size_t vlen = strlen(v);
            if (vlen >= 2 && v[vlen - 1] == v[0])
                v[vlen - 1] = '\0';
        }
        /* Set or add */
        config_set(self, k, v);
    }
    fclose(fp);
    return 0;
}

void config_set(Config* self, const char* key, const char* val) {
    if (!self || !key || !val) return;
    for (size_t i = 0; i < self->count; i++) {
        if (strcmp(self->keys[i], key) == 0) {
            free(self->vals[i]);
            self->vals[i] = _strdup(val);
            return;
        }
    }
    if (self->count >= self->cap) {
        self->cap *= 2;
        self->keys = (char**)realloc(self->keys, self->cap * sizeof(char*));
        self->vals = (char**)realloc(self->vals, self->cap * sizeof(char*));
    }
    self->keys[self->count] = _strdup(key);
    self->vals[self->count] = _strdup(val);
    self->count++;
}

const char* config_get(Config* self, const char* key) {
    if (!self || !key) return NULL;
    for (size_t i = 0; i < self->count; i++)
        if (strcmp(self->keys[i], key) == 0) return self->vals[i];
    return NULL;
}

double config_get_double(Config* self, const char* key, double default_val) {
    const char* v = config_get(self, key);
    if (!v) return default_val;
    return strtod(v, NULL);
}

long config_get_long(Config* self, const char* key, long default_val) {
    const char* v = config_get(self, key);
    if (!v) return default_val;
    return strtol(v, NULL, 10);
}

bool config_get_bool(Config* self, const char* key, bool default_val) {
    const char* v = config_get(self, key);
    if (!v) return default_val;
    return (strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0);
}

size_t config_count(const Config* self) {
    return self ? self->count : 0;
}
