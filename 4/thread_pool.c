#include "thread_pool.h"
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#define TPOOL_ERR_NOT_IMPLEMENTED -1

struct thread_task {
    thread_task_f function;
    void *arg;
};

struct thread_pool {
    pthread_t *threads;
    size_t max_thread_count;
    size_t curr_thread_count;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool terminate;
};

void *thread_function(void *arg) {
    struct thread_pool *pool = (struct thread_pool *)arg;
    while (true) {
        pthread_mutex_lock(&pool->lock);
        while (pool->curr_thread_count == 0 && !pool->terminate) {
            pthread_cond_wait(&pool->cond, &pool->lock);
        }
        if (pool->terminate) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }
        // Здесь можно добавить логику для получения и выполнения задачи
        pthread_mutex_unlock(&pool->lock);
    }
    pthread_exit(NULL);
}

int thread_pool_new(int max_thread_count, struct thread_pool **pool) {
    if (max_thread_count == 0 || pool == NULL)
        return TPOOL_ERR_INVALID_ARGUMENT;

    *pool = (struct thread_pool *)malloc(sizeof(struct thread_pool));
    if (*pool == NULL)
        return -1; // Ошибка выделения памяти

    (*pool)->threads = (pthread_t *)malloc(max_thread_count * sizeof(pthread_t));
    if ((*pool)->threads == NULL) {
        free(*pool);
        return -1; // Ошибка выделения памяти
    }

    (*pool)->max_thread_count = max_thread_count;
    (*pool)->curr_thread_count = 0;
    (*pool)->terminate = false;

    pthread_mutex_init(&(*pool)->lock, NULL);
    pthread_cond_init(&(*pool)->cond, NULL);

    for (int i = 0; i < max_thread_count; ++i) {
        if (pthread_create(&((*pool)->threads[i]), NULL, thread_function, *pool) != 0) {
            for (int j = 0; j < i; ++j) {
                pthread_cancel((*pool)->threads[j]);
            }
            pthread_mutex_destroy(&(*pool)->lock);
            pthread_cond_destroy(&(*pool)->cond);
            free((*pool)->threads);
            free(*pool);
            return -1;
        }
        ++(*pool)->curr_thread_count;
    }

    return 0; // Успех
}

int thread_pool_thread_count(const struct thread_pool *pool) {
    if (pool == NULL)
        return -1; // Некорректный аргумент

    return pool->curr_thread_count;
}

int thread_pool_delete(struct thread_pool *pool) {
    if (pool == NULL)
        return TPOOL_ERR_INVALID_ARGUMENT;

    pthread_mutex_lock(&pool->lock);
    pool->terminate = true;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->lock);

    // Ожидание завершения всех потоков
    for (size_t i = 0; i < pool->max_thread_count; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    // Освобождение памяти и уничтожение мьютекса и условной переменной
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    free(pool->threads);
    free(pool);

    return 0; // Успех
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task) {
    if (pool == NULL || task == NULL)
        return TPOOL_ERR_INVALID_ARGUMENT;

    // Реализация добавления задачи в очередь пула потоков

    return 0; // Успех
}

int thread_task_new(struct thread_task **task, thread_task_f function, void *arg) {
    if (task == NULL)
        return TPOOL_ERR_INVALID_ARGUMENT;

    *task = (struct thread_task *)malloc(sizeof(struct thread_task));
    if (*task == NULL)
        return -1; // Ошибка выделения памяти

    (*task)->function = function;
    (*task)->arg = arg;

    return 0; // Успех
}

bool thread_task_is_finished(const struct thread_task *task) {
    if (task == NULL)
        return false;

    // Здесь может быть реализация проверки завершения задачи.
    // В данной заглушке просто возвращается false.
    return false;
}

bool thread_task_is_running(const struct thread_task *task) {
    if (task == NULL)
        return false;


    return true;
}

int thread_task_join(struct thread_task *task, void **result) {
    if (task == NULL)
        return TPOOL_ERR_INVALID_ARGUMENT;

    return 0;
}

int thread_task_delete(struct thread_task *task) {
    if (task == NULL)
        return TPOOL_ERR_INVALID_ARGUMENT;

    free(task);
    return 0; // Успех
}
