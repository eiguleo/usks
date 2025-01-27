#include <pthread.h>
#define QUEUE_SIZE 5
#define MAX_ARRAY_SIZE 100

// 队列元素结构体
typedef struct {
    char* array;  // 指向数组的指针
    int size;    // 数组的大小
} QueueElement;

// 队列结构体
typedef struct {
    QueueElement elements[QUEUE_SIZE];  // 存储队列元素的数组
    int head;                           // 队头指针
    int tail;                           // 队尾指针
    int count;                          // 队列中元素的个数
    pthread_mutex_t mutex;              // 互斥锁
    pthread_cond_t not_empty;           // 非空条件变量
    pthread_cond_t not_full;            // 非满条件变量
} SyncQueue;


void init_queue(SyncQueue* queue);
void enqueue(SyncQueue* queue, char* array, int size) ;
QueueElement dequeue(SyncQueue* queue) ;
void destroy_queue(SyncQueue* queue) ;




