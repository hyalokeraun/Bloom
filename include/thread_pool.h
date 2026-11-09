#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Task ───────────────────────────────────────────────────── */
typedef void (*TaskFunc)(void* user_data);

/* ── Thread Pool ────────────────────────────────────────────── */
typedef struct ThreadPool ThreadPool;

ThreadPool* thread_pool_create(size_t num_threads);
void        thread_pool_destroy(ThreadPool* self);

/* Submit a task (non-blocking). If the pool is full, blocks until space. */
void thread_pool_submit(ThreadPool* self, TaskFunc func, void* user_data);

/* Wait for all submitted tasks to finish */
void thread_pool_drain(ThreadPool* self);

/* Query */
size_t thread_pool_size(const ThreadPool* self);
bool   thread_pool_is_shutting_down(const ThreadPool* self);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_POOL_H */
