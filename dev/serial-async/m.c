#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#define SERIAL_PORT "/tmp/t1"

int open_serial_port(const char *port)
{
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
        perror("open serial port error");
        return -1;
    }

    struct termios options;
    if (tcgetattr(fd, &options) != 0)
    {
        perror("tcgetattr error");
        close(fd);
        return -1;
    }

    cfsetispeed(&options, B115200);  // 设置输入波特率为115200
    cfsetospeed(&options, B115200);  // 设置输出波特率为115200

    options.c_cflag |= (CLOCAL | CREAD);    // 设置本地模式和使能读取
    options.c_cflag &= ~CSIZE;              // 清除数据位掩码
    options.c_cflag |= CS8;                 // 设置8位数据位
    options.c_cflag &= ~PARENB;             // 清除奇偶校验位
    options.c_cflag &= ~CSTOPB;             // 清除停止位
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // 设置非规范模式，关闭回显等
    options.c_iflag &= ~(IXON | IXOFF | IXANY);          // 关闭软件流控制
    options.c_oflag &= ~OPOST;              // 设置原始输出模式

    if (tcsetattr(fd, TCSANOW, &options) != 0)
    {
        perror("tcsetattr error");
        close(fd);
        return -1;
    }

    return fd;
}

void signal_handler_IO(int fd)
{
    static unsigned char buffer[256];
    int count = read(fd, buffer, sizeof(buffer));
    if (count < 0)
    {
        perror("read error");
    }
    else
    {
        buffer[count] = '\0';
        printf("Received: %s\n", buffer);
    }
}

int set_async_mode(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
    {
        perror("fcntl F_GETFL error");
        return -1;
    }

    flags |= O_ASYNC;  // 设置异步模式
    if (fcntl(fd, F_SETFL, flags) == -1)
    {
        perror("fcntl F_SETFL error");
        return -1;
    }

    if (fcntl(fd, F_SETOWN, getpid()) == -1)
    {
        perror("fcntl F_SETOWN error");
        return -1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler_IO;
    
    if (sigaction(SIGIO, &sa, NULL) == -1)
    {
        perror("sigaction error");
        return -1;
    }

    return 0;
}

int main()
{
    int serial_fd = open_serial_port(SERIAL_PORT);
    if (serial_fd == -1)
    {
        return -1;
    }

    if (set_async_mode(serial_fd) == -1)
    {
        close(serial_fd);
        return -1;
    }

    // 写操作示例
    // const char *data = "Hello, Serial Port!\n";
    // if (write(serial_fd, data, strlen(data)) < 0)
    // {
    //     perror("write error");
    // }

    // 主循环，可以进行其他操作
    while (1)
    {
        // 可以在这里进行其他任务
        sleep(3);
    }

    close(serial_fd);
    return 0;
}