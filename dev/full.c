#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define MAX_QUEUE_SIZE 3
#define BUFFER_SIZE 1024

// 队列结构
typedef struct {
    char data[MAX_QUEUE_SIZE][BUFFER_SIZE]; // 队列数据
    int front;                              // 队头
    int rear;                               // 队尾
    int size;                               // 当前队列大小
    pthread_mutex_t lock;                   // 互斥锁
    pthread_cond_t not_empty;               // 条件变量：队列不为空
    pthread_cond_t not_full;                // 条件变量：队列未满
} Queue;

// 初始化队列
void queue_init(Queue *q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

// 销毁队列
void queue_destroy(Queue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

// 检查队列是否已满
int queue_is_full(Queue *q) {
    return (q->size == MAX_QUEUE_SIZE);
}

// 检查队列是否为空
int queue_is_empty(Queue *q) {
    return (q->size == 0);
}

// 插入数据到队列
int queue_push(Queue *q, const char *data) {
    pthread_mutex_lock(&q->lock);

    // 如果队列已满，等待队列有空闲空间
    while (queue_is_full(q)) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    // 插入数据
    strncpy(q->data[q->rear], data, BUFFER_SIZE);
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->size++;

    // 通知消费者线程
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0; // 插入成功
}

// 从队列中取出数据
int queue_pop(Queue *q, char *data) {
    pthread_mutex_lock(&q->lock);

    // 如果队列为空，等待队列有数据
    while (queue_is_empty(q)) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    // 取出数据
    strncpy(data, q->data[q->front], BUFFER_SIZE);
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;

    // 通知生产者线程
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0; // 取出成功
}

// 测试生产者线程
void *producer(void *arg) {
    Queue *q = (Queue *)arg;
    char data[BUFFER_SIZE];
    int count = 0;

    while (1) {
        snprintf(data, BUFFER_SIZE, "Data %d", count++);
        printf("=> wait befor push\n");
        if (queue_push(q, data) == 0) {
            printf("Produced: %s\n", data);
        }
        sleep(4); // 模拟生产速度
    }
    return NULL;
}

// 测试消费者线程
void *consumer(void *arg) {
    Queue *q = (Queue *)arg;
    char data[BUFFER_SIZE];

    while (1) {
        printf("=> wait befor pop\n");
        if (queue_pop(q, data) == 0) {
            printf("Consumed: %s\n", data);
        }
        sleep(1); // 模拟消费速度
    }
    return NULL;
}

int main() {
    Queue q;
    queue_init(&q);

    // 创建生产者和消费者线程
    pthread_t producer_thread, consumer_thread;
    pthread_create(&producer_thread, NULL, producer, &q);
    pthread_create(&consumer_thread, NULL, consumer, &q);

    // 等待线程结束（永远不会结束，除非手动终止）
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    // 销毁队列
    queue_destroy(&q);
    return 0;
}