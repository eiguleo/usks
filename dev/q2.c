#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>

#define MAX_QUEUE_SIZE 5
#define AGING_TIME 30
#define SERIAL_TIMEOUT 10
#define SOCKET_PATH "/tmp/u1"
#define DEBUG 1

typedef struct {
    char data[256];
    time_t timestamp;
} ListNode;

typedef struct {
    char data[256];
} QueueItem;

typedef struct {
    QueueItem queue[MAX_QUEUE_SIZE];
    int front, rear;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} Queue;

typedef struct {
    ListNode *list;
    int list_size;
    pthread_mutex_t list_lock;
    Queue queue;
    char dev_name[20];
    pthread_t thread_id;
} Device;

Device devices[2]; // Assuming 2 devices /dev/ttyUSB0 and /dev/ttyUSB1

void debug_print(const char *message) {
    if (DEBUG) {
        printf("[DEBUG] %s\n", message);
    }
}

void init_queue(Queue *q) {
    q->front = q->rear = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

int is_queue_full(Queue *q) {
    return (q->rear + 1) % MAX_QUEUE_SIZE == q->front;
}

int is_queue_empty(Queue *q) {
    return q->front == q->rear;
}

void enqueue(Queue *q, const char *data) {
    pthread_mutex_lock(&q->lock);
    while (is_queue_full(q)) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    strcpy(q->queue[q->rear].data, data);
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

int dequeue(Queue *q, char *data) {
    pthread_mutex_lock(&q->lock);
    while (is_queue_empty(q)) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    strcpy(data, q->queue[q->front].data);
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

void init_list(ListNode **list, int *size) {
    *list = NULL;
    *size = 0;
}

void add_to_list(ListNode **list, int *size, const char *data) {
    pthread_mutex_lock(&devices[0].list_lock); // Assuming all devices share the same list lock
    *list = realloc(*list, (*size + 1) * sizeof(ListNode));
    strcpy((*list)[*size].data, data);
    (*list)[*size].timestamp = time(NULL);
    (*size)++;
    pthread_mutex_unlock(&devices[0].list_lock);
}

void remove_old_nodes(ListNode **list, int *size) {
    pthread_mutex_lock(&devices[0].list_lock);
    time_t now = time(NULL);
    int i = 0;
    while (i < *size) {
        if (now - (*list)[i].timestamp > AGING_TIME) {
            int j ;
            for (j = i; j < *size - 1; j++) {
                (*list)[j] = (*list)[j + 1];
            }
            (*size)--;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&devices[0].list_lock);
}

void *device_thread(void *arg) {
    Device *dev = (Device *)arg;
    char data[256];
    int fd;
    struct termios tty;
    fd_set read_fds;
    struct timeval timeout;

    while (1) {
        if (dequeue(&dev->queue, data)) {
            debug_print("Dequeued from device queue");

            fd = open(dev->dev_name, O_RDWR | O_NOCTTY | O_SYNC);
            if (fd < 0) {
                perror("open");
                continue;
            }

            memset(&tty, 0, sizeof tty);
            if (tcgetattr(fd, &tty) != 0) {
                perror("tcgetattr");
                close(fd);
                continue;
            }

            cfsetospeed(&tty, B9600);
            cfsetispeed(&tty, B9600);

            tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
            tty.c_iflag &= ~IGNBRK;
            tty.c_lflag = 0;
            tty.c_oflag = 0;
            tty.c_cc[VMIN] = 1;
            tty.c_cc[VTIME] = 0;

            if (tcsetattr(fd, TCSANOW, &tty) != 0) {
                perror("tcsetattr");
                close(fd);
                continue;
            }

            char send_data[256];
            sprintf(send_data, "%s\r", data);
            write(fd, send_data, strlen(send_data));
            debug_print("Sent to serial port");

            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);
            timeout.tv_sec = SERIAL_TIMEOUT;
            timeout.tv_usec = 0;

            int ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
            if (ret == -1) {
                perror("select");
                close(fd);
                continue;
            } else if (ret == 0) {
                debug_print("Serial port timeout");
                add_to_list(&dev->list, &dev->list_size, "timeout");
                close(fd);
                continue;
            }

            char read_buf[256];
            int n = read(fd, read_buf, sizeof(read_buf));
            if (n > 0) {
                read_buf[n] = '\0';
                debug_print("Received from serial port");
                char list_data[512];
                sprintf(list_data, "%s:%s", data, read_buf);
                add_to_list(&dev->list, &dev->list_size, list_data);
            }

            close(fd);
        }
    }
    return NULL;
}

void handle_client_request(int sockfd, const char *request) {
    char cmd[10], data[256];
    sscanf(request, "%[^,],%s", cmd, data);

    if (strcmp(cmd, "0") == 0 || strcmp(cmd, "1") == 0) {
        int dev_index = atoi(cmd);
        Device *dev = &devices[dev_index];

        if (strcmp(data, "dump") == 0) {
            pthread_mutex_lock(&dev->list_lock);
            char response[1024] = "";
            int i;
            for ( i = 0; i < dev->list_size; i++) {
                char entry[256];
                sprintf(entry, "%s (age: %ld sec)\n", dev->list[i].data, time(NULL) - dev->list[i].timestamp);
                strcat(response, entry);
            }
            pthread_mutex_unlock(&dev->list_lock);
            send(sockfd, response, strlen(response), 0);
            debug_print("Dumped list content");
        } else {
            pthread_mutex_lock(&dev->list_lock);
            int found = 0;
            int i;
            for ( i = 0; i < dev->list_size; i++) {
                if (strstr(dev->list[i].data, data) != NULL) {
                    send(sockfd, dev->list[i].data, strlen(dev->list[i].data), 0);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&dev->list_lock);

            if (!found) {
                pthread_t tid;
                pthread_create(&tid, NULL, (void *(*)(void *))enqueue, (void *)&dev->queue);
                pthread_detach(tid);
                debug_print("Enqueued data");
                send(sockfd, "full", 4, 0);
            }
        }
    }
}

int main() {
    int sockfd;
    struct sockaddr_un addr;

    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    int i;

    for ( i = 0; i < 1; i++) {
        sprintf(devices[i].dev_name, "/dev/ttyUSB%d", i);
        init_queue(&devices[i].queue);
        init_list(&devices[i].list, &devices[i].list_size);
        pthread_mutex_init(&devices[i].list_lock, NULL);
        pthread_create(&devices[i].thread_id, NULL, device_thread, &devices[i]);
    }

    while (1) {
        char buffer[256];
        struct sockaddr_un client_addr;
        socklen_t len = sizeof(client_addr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &len);
        if (n > 0) {
            buffer[n] = '\0';
            debug_print("Received from unix socket ");
            printf("=> %s\n", buffer);
            handle_client_request(sockfd, buffer);
        }
    }

    close(sockfd);
    return 0;
}