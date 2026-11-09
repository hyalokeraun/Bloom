#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration store ────────────────────────────────────── */
typedef struct Config Config;

Config* config_create(void);
void    config_destroy(Config* self);

int     config_load(Config* self, const char* file_path);

const char* config_get(Config* self, const char* key);
double      config_get_double(Config* self, const char* key, double default_val);
long        config_get_long(Config* self, const char* key, long default_val);
bool        config_get_bool(Config* self, const char* key, bool default_val);
void        config_set(Config* self, const char* key, const char* val);

size_t config_count(const Config* self);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
