#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <limits.h>
#include <stdarg.h>

#define FIFO_FILE1 "/tmp/myfifo1"
#define FIFO_FILE2 "/tmp/myfifo2"
#define FIFO_FILE3 "/tmp/myfifo3"
#define BUF_SIZE 1024


int max_of_ints(int n, ...) {
    va_list args;
    va_start(args, n); // 初始化va_list变量，指向第一个变参

    int max = INT_MIN; // 初始化最大值为int类型的最小值

    for (int i = 0; i < n; i++) {
        int num = va_arg(args, int); // 获取下一个int类型的参数
        if (num > max) {
            max = num;
        }
    }

    va_end(args); // 结束变参的使用
    return max;
}


int main() {
    int fd1, fd2, fd3;
    char buf[BUF_SIZE];
    fd_set read_fds;
    int max_fd;

    // Create the named pipes (FIFOs) if they don't exist
    if (mkfifo(FIFO_FILE1, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if (mkfifo(FIFO_FILE2, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if (mkfifo(FIFO_FILE3, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    // Open the FIFOs for reading
    fd1 = open(FIFO_FILE1, O_RDONLY | O_NONBLOCK);
    if (fd1 == -1) {
        perror("open FIFO_FILE1");
        exit(EXIT_FAILURE);
    }

    fd2 = open(FIFO_FILE2, O_RDONLY | O_NONBLOCK);
    if (fd2 == -1) {
        perror("open FIFO_FILE2");
        close(fd1);
        exit(EXIT_FAILURE);
    }
    fd3 = open(FIFO_FILE3, O_RDONLY | O_NONBLOCK);
    if (fd2 == -1) {
        perror("open FIFO_FILE2");
        close(fd1);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for clients to send messages...\n");

    while (1) {
        // Clear the set and add file descriptors
        FD_ZERO(&read_fds);
        FD_SET(fd1, &read_fds);
        FD_SET(fd2, &read_fds);
        FD_SET(fd3, &read_fds);

        // Determine the maximum file descriptor number
        max_fd = (fd1 > fd2) ? fd1 : fd2;
        max_fd = max_of_ints(fd1,fd2,fd3);

        // Wait for data on any of the file descriptors
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity == -1) {
            perror("select");
            close(fd1);
            close(fd2);
            exit(EXIT_FAILURE);
        }

        // Check if there is data to read from fd1
        if (FD_ISSET(fd1, &read_fds)) {
            ssize_t bytesRead = read(fd1, buf, BUF_SIZE - 1);
            if (bytesRead > 0) {
                buf[bytesRead] = '\0';
                printf("Received from FIFO1: %s\n", buf);
            } else if (bytesRead == -1) {
                perror("read FIFO1");
                close(fd1);
                exit(EXIT_FAILURE);
            }
        }

        // Check if there is data to read from fd2
        if (FD_ISSET(fd2, &read_fds)) {
            ssize_t bytesRead = read(fd2, buf, BUF_SIZE - 1);
            if (bytesRead > 0) {
                buf[bytesRead] = '\0';
                printf("Received from FIFO2: %s\n", buf);
            } else if (bytesRead == -1) {
                perror("read FIFO2");
                close(fd2);
                exit(EXIT_FAILURE);
            }
        }
        if (FD_ISSET(fd3, &read_fds)) {
            ssize_t bytesRead = read(fd3, buf, BUF_SIZE - 1);
            if (bytesRead > 0) {
                buf[bytesRead] = '\0';
                printf("Received from FIFO3: %s\n", buf);
            } else if (bytesRead == -1) {
                perror("read FIFO3");
                close(fd3);
                exit(EXIT_FAILURE);
            }
        }
    }

    // Close the FIFOs
    close(fd1);
    close(fd2);
    close(fd3);
    return 0;
}