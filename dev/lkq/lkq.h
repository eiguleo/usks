// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include <pthread.h>
// #include <time.h>
// #include <semaphore.h>

#define MAX_QUEUE_SIZE 3
#define MAX_STRING_LENGTH 100

typedef struct {
    char data[MAX_QUEUE_SIZE][MAX_STRING_LENGTH];
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} Queue;

void queue_init(Queue *q) ;
int queue_is_full(Queue *q) ;
int queue_is_empty(Queue *q) ;
int queue_push(Queue *q, const char *str, const struct timespec *timeout) ;
int queue_pop(Queue *q, char *str) ;
int queue_search(Queue *q, const char *str) ;
void queue_destroy(Queue *q) ;