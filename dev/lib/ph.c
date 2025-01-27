#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// 共享资源
int shared_data = 0;
// 互斥锁
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// 线程函数
void* thread_func1(void* arg){
    int loop;
    for(loop = 0; loop < 100000; ++loop){
        printf("=> func 1 loop: %d...\n",loop);
        pthread_mutex_lock(&mutex); // 加锁
        shared_data++; // 修改共享资源
        printf("=> func 1 shared_data: %d...\n",shared_data);        
        sleep(1);
        pthread_mutex_unlock(&mutex); // 解锁
    }
    return NULL;
}
void* thread_func2(void* arg){
    int loop;
    for(loop = 0; loop < 100000; ++loop){
        printf("=> func 2 loop: %d...\n",loop);
        pthread_mutex_lock(&mutex); // 加锁
        shared_data++; // 修改共享资源
        printf("=> func 2 shared_data: %d...\n",shared_data);
        sleep(2);
        pthread_mutex_unlock(&mutex); // 解锁
    }
    return NULL;
}

int main(){
    pthread_t tid1, tid2;

    // 创建两个线程
    pthread_create(&tid1, NULL, thread_func1, NULL);
    pthread_create(&tid2, NULL, thread_func2, NULL);

    // 等待线程结束
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    // 输出共享资源的最终值
    printf("shared_data = %d\n", shared_data);

    // 销毁互斥锁
    pthread_mutex_destroy(&mutex);

    return 0;
}