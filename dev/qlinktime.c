#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>

#define MAX_QUEUE_SIZE 4
#define SOCKET_PATH "/tmp/u1"
#define TIMEOUT 5
#define BUFFER_SIZE 256
#define AGING_TIME 600 // 老化时间为 10 分钟（600 秒）

typedef struct ListNode {
    char data[BUFFER_SIZE];
    time_t timestamp; // 记录插入时间
    struct ListNode* next;
} ListNode;

typedef struct {
    char data[BUFFER_SIZE];
    struct sockaddr_un client_addr; // 客户端地址
    socklen_t client_len;           // 客户端地址长度
} QueueItem;

QueueItem queue[MAX_QUEUE_SIZE];
int queue_size = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

ListNode* head = NULL; // 链表头
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// 初始化链表
void init_list() {
    head = NULL;
}

// 重置老化时间
void reset_aging_time(const char* data) {
    pthread_mutex_lock(&list_mutex);

    ListNode* current = head;
    while (current != NULL) {
        if (strcmp(current->data, data) == 0) {
            // 重置老化时间
            current->timestamp = time(NULL);
            break;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&list_mutex);
}

// 插入链表
void insert_list(const char* data) {
    pthread_mutex_lock(&list_mutex);

    // 创建新节点
    ListNode* new_node = (ListNode*)malloc(sizeof(ListNode));
    strncpy(new_node->data, data, BUFFER_SIZE);
    new_node->timestamp = time(NULL);
    new_node->next = head;
    head = new_node;

    pthread_mutex_unlock(&list_mutex);
}

// 查询链表
int search_list(const char* data) {
    pthread_mutex_lock(&list_mutex);

    ListNode* current = head;
    while (current != NULL) {
        if (strcmp(current->data, data) == 0) {
            pthread_mutex_unlock(&list_mutex);
            return 1; // 找到
        }
        current = current->next;
    }

    pthread_mutex_unlock(&list_mutex);
    return 0; // 未找到
}

// 老化机制：删除超过 AGING_TIME 的节点
void aging_list() {
    pthread_mutex_lock(&list_mutex);

    ListNode* current = head;
    ListNode* prev = NULL;
    time_t now = time(NULL);

    while (current != NULL) {
        if (now - current->timestamp > AGING_TIME) {
            // 删除节点
            if (prev == NULL) {
                head = current->next;
                free(current);
                current = head;
            } else {
                prev->next = current->next;
                free(current);
                current = prev->next;
            }
        } else {
            prev = current;
            current = current->next;
        }
    }

    pthread_mutex_unlock(&list_mutex);
}

// 处理队列的线程
void* process_queue(void* arg) {
    int server_fd = *(int*)arg;

    while (1) {
        pthread_mutex_lock(&queue_mutex);
        while (queue_size == 0) {
            pthread_cond_wait(&queue_not_full, &queue_mutex);
        }

        // 处理队列中的消息
        for (int i = 0; i < queue_size; i++) {
            printf("Processing: %s\n", queue[i].data);

            // 在消息末尾加上 'a'
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "%sa", queue[i].data);
            printf("Sending response: %s\n", response);

            // 如果链表中已存在该消息，重置老化时间；否则插入链表
            if (search_list(response)) {
                reset_aging_time(response);
            } else {
                insert_list(response);
            }

            // 返回处理后的消息给客户端
            sendto(server_fd, response, strlen(response), 0,
                   (struct sockaddr*)&queue[i].client_addr, queue[i].client_len);
        }

        queue_size = 0; // 清空队列
        pthread_cond_broadcast(&queue_not_full);
        pthread_mutex_unlock(&queue_mutex);

        sleep(1); // 模拟延迟
    }
    return NULL;
}

// 处理客户端消息
void* handle_client(void* arg) {
    char msg[BUFFER_SIZE];
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    int server_fd = *(int*)arg;

    // 接收消息
    int len = recvfrom(server_fd, msg, sizeof(msg) - 1, 0, (struct sockaddr*)&client_addr, &client_len);
    if (len < 0) {
        perror("recvfrom");
        return NULL;
    }
    msg[len] = '\0';

    // 检查是否为 "dump" 消息
    if (strcmp(msg, "dump") == 0) {
        pthread_mutex_lock(&list_mutex);

        char response[BUFFER_SIZE * MAX_QUEUE_SIZE];
        response[0] = '\0'; // 初始化响应缓冲区

        // 遍历链表，拼接所有内容
        ListNode* current = head;
        time_t now = time(NULL);
        while (current != NULL) {
            // 计算剩余老化时间
            time_t remaining_time = AGING_TIME - (now - current->timestamp);
            if (remaining_time < 0) {
                remaining_time = 0; // 如果已经超时，剩余时间为 0
            }

            // 拼接数据及其剩余老化时间
            char entry[BUFFER_SIZE];
            snprintf(entry, sizeof(entry), "%s (remaining: %ld seconds)\n", current->data, remaining_time);
            strncat(response, entry, BUFFER_SIZE * MAX_QUEUE_SIZE - strlen(response) - 1);

            current = current->next;
        }

        // 如果链表为空，返回 "empty"
        if (head == NULL) {
            strncpy(response, "empty", BUFFER_SIZE * MAX_QUEUE_SIZE);
        }

        sendto(server_fd, response, strlen(response), 0, (struct sockaddr*)&client_addr, client_len);
        pthread_mutex_unlock(&list_mutex);
        return NULL;
    }

    // 查询链表
    if (search_list(msg)) {
        // 如果链表中已存在该消息，重置其老化时间
        // reset_aging_time(msg);
        sendto(server_fd, msg, strlen(msg), 0, (struct sockaddr*)&client_addr, client_len);
        return NULL;
    }

    // 尝试插入队列
    pthread_mutex_lock(&queue_mutex);
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += TIMEOUT;

    while (queue_size >= MAX_QUEUE_SIZE) {
        if (pthread_cond_timedwait(&queue_not_full, &queue_mutex, &timeout) == ETIMEDOUT) {
            printf("Timeout while waiting to insert into queue\n");
            sendto(server_fd, "timeout", 7, 0, (struct sockaddr*)&client_addr, client_len);
            pthread_mutex_unlock(&queue_mutex);
            return NULL;
        }
    }

    // 插入队列
    strncpy(queue[queue_size].data, msg, BUFFER_SIZE);
    queue[queue_size].client_addr = client_addr; // 保存客户端地址
    queue[queue_size].client_len = client_len;   // 保存客户端地址长度
    queue_size++;

    pthread_cond_signal(&queue_not_full);
    pthread_mutex_unlock(&queue_mutex);

    return NULL;
}

// 老化机制线程
void* aging_thread(void* arg) {
    while (1) {
        sleep(AGING_TIME / 2); // 每 5 分钟检查一次
        aging_list();
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_un addr;
    pthread_t queue_thread, aging_thread_id;

    // 初始化链表
    init_list();

    // 创建 Unix 域套接字
    if ((server_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 绑定套接字
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on %s\n", SOCKET_PATH);

    // 创建队列处理线程
    pthread_create(&queue_thread, NULL, process_queue, &server_fd);

    // 创建老化机制线程
    pthread_create(&aging_thread_id, NULL, aging_thread, NULL);

    while (1) {
        // 处理客户端消息
        handle_client(&server_fd);
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}