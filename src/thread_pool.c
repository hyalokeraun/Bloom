#define _CRT_SECURE_NO_WARNINGS
#include "thread_pool.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h>

typedef struct {
    TaskFunc func;
    void* user_data;
} Task;

struct ThreadPool {
    Task* tasks;
    size_t task_count;
    size_t task_cap;
    size_t num_threads;
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE work_cv;
    HANDLE done_event;
    volatile long shutting_down;
    HANDLE* threads;
};

static unsigned int __stdcall worker_thread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    while (1) {
        EnterCriticalSection(&pool->lock);
        while (pool->task_count == 0 && !pool->shutting_down)
            SleepConditionVariableCS(&pool->work_cv, &pool->lock, INFINITE);

        if (pool->shutting_down && pool->task_count == 0) {
            LeaveCriticalSection(&pool->lock);
            break;
        }

        if (pool->task_count > 0) {
            Task t = pool->tasks[0];
            for (size_t i = 1; i < pool->task_count; i++)
                pool->tasks[i - 1] = pool->tasks[i];
            pool->task_count--;
            LeaveCriticalSection(&pool->lock);
            t.func(t.user_data);
            SetEvent(pool->done_event);
        } else {
            LeaveCriticalSection(&pool->lock);
        }
    }
    return 0;
}

ThreadPool* thread_pool_create(size_t num_threads) {
    if (num_threads == 0) num_threads = 4;
    ThreadPool* pool = (ThreadPool*)calloc(1, sizeof(ThreadPool));
    pool->task_cap = 256;
    pool->tasks = (Task*)calloc(pool->task_cap, sizeof(Task));
    pool->num_threads = num_threads;
    pool->done_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    InitializeCriticalSection(&pool->lock);
    InitializeConditionVariable(&pool->work_cv);
    pool->threads = (HANDLE*)calloc(num_threads, sizeof(HANDLE));
    for (size_t i = 0; i < num_threads; i++) {
        pool->threads[i] = (HANDLE)_beginthreadex(NULL, 0, worker_thread, pool, 0, NULL);
    }
    return pool;
}

void thread_pool_destroy(ThreadPool* self) {
    if (!self) return;
    InterlockedExchange((volatile LONG*)&self->shutting_down, 1);
    WakeAllConditionVariable(&self->work_cv);
    for (size_t i = 0; i < self->num_threads; i++) {
        if (self->threads[i]) {
            WaitForSingleObject(self->threads[i], 5000);
            CloseHandle(self->threads[i]);
        }
    }
    free(self->threads);
    free(self->tasks);
    DeleteCriticalSection(&self->lock);
    CloseHandle(self->done_event);
    free(self);
}

void thread_pool_submit(ThreadPool* self, TaskFunc func, void* user_data) {
    if (!self) return;
    EnterCriticalSection(&self->lock);
    if (self->task_count >= self->task_cap) {
        self->task_cap *= 2;
        self->tasks = (Task*)realloc(self->tasks, self->task_cap * sizeof(Task));
    }
    self->tasks[self->task_count].func = func;
    self->tasks[self->task_count].user_data = user_data;
    self->task_count++;
    WakeConditionVariable(&self->work_cv);
    LeaveCriticalSection(&self->lock);
}

void thread_pool_drain(ThreadPool* self) {
    if (!self) return;
    EnterCriticalSection(&self->lock);
    while (self->task_count > 0) {
        LeaveCriticalSection(&self->lock);
        WaitForSingleObject(self->done_event, INFINITE);
        ResetEvent(self->done_event);
        EnterCriticalSection(&self->lock);
    }
    LeaveCriticalSection(&self->lock);
}

size_t thread_pool_size(const ThreadPool* self) {
    return self ? self->num_threads : 0;
}

bool thread_pool_is_shutting_down(const ThreadPool* self) {
    if (!self) return false;
    return (bool)self->shutting_down;
}
