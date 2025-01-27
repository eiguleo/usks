#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "./queue.h"

// 初始化队列
void init_queue(SyncQueue* queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

// 入队操作
void enqueue(SyncQueue* queue, char* array, int size) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == QUEUE_SIZE) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    queue->elements[queue->tail].array = (char*)malloc(size * sizeof(char));
    for (int i = 0; i < size; i++) {
        queue->elements[queue->tail].array[i] = array[i];
    }
    queue->elements[queue->tail].size = size;
    queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    // printf("=> enqueue done\n");
}

// 出队操作
QueueElement dequeue(SyncQueue* queue) {
    QueueElement element;
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    element = queue->elements[queue->head];
    queue->head = (queue->head + 1) % QUEUE_SIZE;
    queue->count--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    // printf("=> dequeue done\n");
    return element;
}

// 销毁队列
void destroy_queue(SyncQueue* queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    for (int i = 0; i < QUEUE_SIZE; i++) {
        free(queue->elements[i].array);
    }
}

