#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "/home/huzh/tmp/c-exer/dev/lkq/lkq.h"

#define MAX_BUFFER_SIZE 1024
#define SOCKET_PATH1 "/tmp/u1"

Queue queue;

void *thread_func(void *arg) {

    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    strncpy(buffer, (char *)arg, sizeof(buffer) - 1);
    // 将数据放入队列
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5;  // 5 seconds timeout
    if (queue_push(&queue, buffer, &timeout) == 0) {
        printf("Pushed %s to queue\n", buffer);
    } else {
        printf("Failed to push %s to queue (timeout)\n", buffer);
    }

    // 发送数据回去
    // if (sendto(sockfd, buffer, bytes_received, 0,
    //            (struct sockaddr *)&client_addr, client_len) < 0) {
    //     perror("sendto");
    // }

    pthread_exit(NULL);
}

int main() {
    queue_init(&queue);
    int sockfd1;
    struct sockaddr_un addr1;

    // 创建第一个套接字
    sockfd1 = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd1 < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr1, 0, sizeof(struct sockaddr_un));
    addr1.sun_family = AF_UNIX;
    strncpy(addr1.sun_path, SOCKET_PATH1, sizeof(addr1.sun_path) - 1);

    // 绑定第一个套接字
    unlink(SOCKET_PATH1);  // 确保路径没有被占用
    if (bind(sockfd1, (struct sockaddr *)&addr1, sizeof(struct sockaddr_un)) < 0) {
        perror("bind");
        close(sockfd1);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on %s ...\n", SOCKET_PATH1);

    int sockfd = sockfd1;
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_received;

    while (1) {
        // 接收数据
        bytes_received = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (bytes_received < 0) {
            perror("recvfrom");
            pthread_exit(NULL);
        }

        // 打印接收到的数据
        buffer[bytes_received] = '\0';
        printf("Received: %s\n", buffer);
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, thread_func, (void*)(buffer)) != 0) {
            perror("pthread_create");
        } else {
            pthread_detach(thread_id);
        }
    }

    // 关闭套接字
    close(sockfd1);
    unlink(SOCKET_PATH1);

    return 0;
}