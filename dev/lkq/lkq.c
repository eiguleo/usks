#include "lkq.h"
#include <errno.h>
// #include <stdio.h>
// #include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>


void queue_init(Queue *q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

int queue_is_full(Queue *q) {
    return q->size == MAX_QUEUE_SIZE;
}

int queue_is_empty(Queue *q) {
    return q->size == 0;
}

int queue_push(Queue *q, const char *str, const struct timespec *timeout) {
    pthread_mutex_lock(&q->lock);

    while (queue_is_full(q)) {
        if (timeout != NULL) {
            if (pthread_cond_timedwait(&q->not_full, &q->lock, timeout) != 0) {
                pthread_mutex_unlock(&q->lock);
                return -1; // Timeout
            }
        } else {
            pthread_cond_wait(&q->not_full, &q->lock);
        }
    }

    strncpy(q->data[q->rear], str, MAX_STRING_LENGTH);
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);

    return 0;
}

int queue_pop(Queue *q, char *str) {
    pthread_mutex_lock(&q->lock);

    while (queue_is_empty(q)) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    strncpy(str, q->data[q->front], MAX_STRING_LENGTH);
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);

    return 0;
}

int queue_search(Queue *q, const char *str) {
    pthread_mutex_lock(&q->lock);

    int index = q->front;
    for (int i = 0; i < q->size; i++) {
        if (strcmp(q->data[index], str) == 0) {
            pthread_mutex_unlock(&q->lock);
            return 1; // Found
        }
        index = (index + 1) % MAX_QUEUE_SIZE;
    }

    pthread_mutex_unlock(&q->lock);
    return 0; // Not found
}

void queue_destroy(Queue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}