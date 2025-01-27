#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_QUEUE_SIZE 5
#define SOCKET_PATH "/tmp/u1"
#define TIMEOUT 5
#define BUFFER_SIZE 256
#define AGING_TIME 30 // 老化时间为 30 秒
#define SERIAL_TIMEOUT 10 // 串口读取超时时间为 10 秒
#define BAUDRATE B9600 // 波特率为 9600

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

typedef struct {
    int tty_fd; // 串口文件描述符
    QueueItem queue[MAX_QUEUE_SIZE];
    int queue_size;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_not_full;
    ListNode* head; // 链表头
    pthread_mutex_t list_mutex;
} Device;

Device devices[10]; // 假设最多支持 10 个 /dev/ttyUSBX 设备
pthread_t device_threads[10]; // 每个设备的处理线程

// 初始化链表
void init_list(ListNode** head) {
    *head = NULL;
}

// 重置老化时间
void reset_aging_time(ListNode** head, const char* data) {
    pthread_mutex_lock(&devices[0].list_mutex); // 假设只有一个设备

    ListNode* current = *head;
    while (current != NULL) {
        if (strcmp(current->data, data) == 0) {
            // 重置老化时间
            current->timestamp = time(NULL);
            break;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&devices[0].list_mutex);
}

// 插入链表
void insert_list(ListNode** head, const char* data) {
    pthread_mutex_lock(&devices[0].list_mutex); // 假设只有一个设备

    // 创建新节点
    ListNode* new_node = (ListNode*)malloc(sizeof(ListNode));
    strncpy(new_node->data, data, BUFFER_SIZE);
    new_node->timestamp = time(NULL);
    new_node->next = *head;
    *head = new_node;

    pthread_mutex_unlock(&devices[0].list_mutex);
}

// 查询链表
int search_list(ListNode* head, const char* data) {
    pthread_mutex_lock(&devices[0].list_mutex); // 假设只有一个设备

    ListNode* current = head;
    while (current != NULL) {
        if (strcmp(current->data, data) == 0) {
            pthread_mutex_unlock(&devices[0].list_mutex);
            return 1; // 找到
        }
        current = current->next;
    }

    pthread_mutex_unlock(&devices[0].list_mutex);
    return 0; // 未找到
}

// 老化机制：删除超过 AGING_TIME 的节点
void aging_list(ListNode** head) {
    pthread_mutex_lock(&devices[0].list_mutex); // 假设只有一个设备

    ListNode* current = *head;
    ListNode* prev = NULL;
    time_t now = time(NULL);

    while (current != NULL) {
        if (now - current->timestamp > AGING_TIME) {
            // 删除节点
            if (prev == NULL) {
                *head = current->next;
                free(current);
                current = *head;
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

    pthread_mutex_unlock(&devices[0].list_mutex);
}

// 打开串口设备
int open_serial(const char* device) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10; // 读取超时时间为 1 秒

    tcsetattr(fd, TCSANOW, &options);
    return fd;
}

// 处理设备的线程
void* process_device(void* arg) {
    int index = *(int*)arg;
    Device* device = &devices[index];

    while (1) {
        pthread_mutex_lock(&device->queue_mutex);
        while (device->queue_size == 0) {
            pthread_cond_wait(&device->queue_not_full, &device->queue_mutex);
        }

        // 处理队列中的消息
        int i=0;
        for (; i < device->queue_size; i++) {
            printf("Processing: %s\n", device->queue[i].data);

            // 清空串口输出缓存
            tcflush(device->tty_fd, TCOFLUSH);

            // 发送数据到串口
            printf("Writing to serial: %s\n", device->queue[i].data);
            write(device->tty_fd, device->queue[i].data, strlen(device->queue[i].data));

            // 尝试读取串口数据
            char response[BUFFER_SIZE];
            int len = 0;
            time_t start = time(NULL);
            while (time(NULL) - start < SERIAL_TIMEOUT) {
                len = read(device->tty_fd, response, BUFFER_SIZE - 1);
                if (len > 0) {
                    response[len] = '\0';
                    break;
                }
                usleep(100000); // 100ms
            }

            // 如果超时，返回 timeout
            if (len <= 0) {
                strcpy(response, "timeout");
            }

            // 将结果插入链表
            insert_list(&device->head, response);

            // 返回结果给客户端
            sendto(device->tty_fd, response, strlen(response), 0,
                   (struct sockaddr*)&device->queue[i].client_addr, device->queue[i].client_len);
        }

        device->queue_size = 0; // 清空队列
        pthread_cond_broadcast(&device->queue_not_full);
        pthread_mutex_unlock(&device->queue_mutex);

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

    // 解析消息
    int device_index;
    char command[BUFFER_SIZE];
    if (sscanf(msg, "%d,%s", &device_index, command) != 2) {
        sendto(server_fd, "invalid format", 14, 0, (struct sockaddr*)&client_addr, client_len);
        return NULL;
    }

    // 检查设备索引是否有效
    if (device_index < 0 || device_index >= 10 || devices[device_index].tty_fd == -1) {
        sendto(server_fd, "invalid device", 14, 0, (struct sockaddr*)&client_addr, client_len);
        return NULL;
    }

    Device* device = &devices[device_index];

    // 处理 "dump" 消息
    if (strcmp(command, "dump") == 0) {
        pthread_mutex_lock(&device->list_mutex);

        char response[BUFFER_SIZE * MAX_QUEUE_SIZE];
        response[0] = '\0'; // 初始化响应缓冲区

        // 遍历链表，拼接所有内容
        ListNode* current = device->head;
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
        if (device->head == NULL) {
            strncpy(response, "empty", BUFFER_SIZE * MAX_QUEUE_SIZE);
        }

        sendto(server_fd, response, strlen(response), 0, (struct sockaddr*)&client_addr, client_len);
        pthread_mutex_unlock(&device->list_mutex);
        return NULL;
    }

    // 查询链表
    if (search_list(device->head, command)) {
        sendto(server_fd, command, strlen(command), 0, (struct sockaddr*)&client_addr, client_len);
        return NULL;
    }

    // 尝试插入队列
    pthread_mutex_lock(&device->queue_mutex);
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += TIMEOUT;

    while (device->queue_size >= MAX_QUEUE_SIZE) {
        if (pthread_cond_timedwait(&device->queue_not_full, &device->queue_mutex, &timeout) == ETIMEDOUT) {
            printf("Timeout while waiting to insert into queue\n");
            sendto(server_fd, "full", 4, 0, (struct sockaddr*)&client_addr, client_len);
            pthread_mutex_unlock(&device->queue_mutex);
            return NULL;
        }
    }

    // 插入队列
    strncpy(device->queue[device->queue_size].data, command, BUFFER_SIZE);
    device->queue[device->queue_size].client_addr = client_addr; // 保存客户端地址
    device->queue[device->queue_size].client_len = client_len;   // 保存客户端地址长度
    device->queue_size++;

    pthread_cond_signal(&device->queue_not_full);
    pthread_mutex_unlock(&device->queue_mutex);

    return NULL;
}

// 初始化设备
void init_devices() {
    int i=0;
    for (; i < 1; i++) {
        char device_name[20];
        snprintf(device_name, sizeof(device_name), "/dev/ttyUSB%d", i);

        devices[i].tty_fd = open_serial(device_name);
        if (devices[i].tty_fd == -1) {
            printf("Failed to open %s\n", device_name);
            continue;
        }

        init_list(&devices[i].head);
        devices[i].queue_size = 0;
        pthread_mutex_init(&devices[i].queue_mutex, NULL);
        pthread_cond_init(&devices[i].queue_not_full, NULL);
        pthread_mutex_init(&devices[i].list_mutex, NULL);

        // 创建设备处理线程
        int* index = malloc(sizeof(int));
        *index = i;
        pthread_create(&device_threads[i], NULL, process_device, index);
    }
}

int main() {
    int server_fd;
    struct sockaddr_un addr;

    // 初始化设备
    init_devices();

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

    while (1) {
        // 处理客户端消息
        handle_client(&server_fd);
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}