#include <errno.h>
#include <fcntl.h>
#include <json-c/printbuf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

int set_serial_port_attributes(int fd, int baud_rate) {
  struct termios tty;

  if (tcgetattr(fd, &tty) != 0) {
    perror("tcgetattr");
    return -1;
  }

  cfsetospeed(&tty, baud_rate);
  cfsetispeed(&tty, baud_rate);

  tty.c_cflag &= ~PARENB; // 无奇偶校验
  tty.c_cflag &= ~CSTOPB; // 1个停止位
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;            // 8个数据位
  tty.c_cflag &= ~CRTSCTS;       // 无硬件流控制
  tty.c_cflag |= CREAD | CLOCAL; // 启用接收器，忽略挂起

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // 无软件流控制
  tty.c_iflag &= ~(ICRNL | INLCR);        // 无回车换行转换
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INPCK); // 无特殊字符处理

  tty.c_lflag &= ~ICANON;
  tty.c_lflag &= ~ECHO;   // 关闭回显
  tty.c_lflag &= ~ISIG;   // 忽略信号字符
  tty.c_lflag &= ~IEXTEN; // 关闭非标准扩展

  tty.c_oflag &= ~OPOST; // 无输出处理

  tty.c_cc[VMIN] = 1;   // 非阻塞读
  tty.c_cc[VTIME] = 10; // 1秒超时

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    perror("tcsetattr");
    return -1;
  }

  return 0;
}

int main() {
  char *usbdev_s = "/dev/pts/8";
  int baud_rate = 115200;
  fd_set readfds;
  struct timeval tv;
  int len;
  printf("=> bp1\n");
  int ret;
  for (;;) {
    printf("=> bp2\n");
    int fd = open(usbdev_s, O_RDWR | O_NOCTTY | O_SYNC);

    if (fd < 0) {
      perror("open");
      return -1;
    }

    if (set_serial_port_attributes(fd, baud_rate) < 0) {
      printf("=> set serial\n");
      close(fd);
      return -1;
    }

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    printf("=> bp3\n");
    // int timeout_ms = 5;
    // tv.tv_sec = timeout_ms / 1000;
    // tv.tv_usec = (timeout_ms % 1000) * 1000;
    ret = select(fd + 1, &readfds, NULL, NULL, NULL);
    if (ret == -1) {
      perror("select");
      return -1;
    } else if (ret == 0) {
      perror("timeout");
      return 0;
    }
    printf("=> bp4\n");
    char buffer[100];
    if (FD_ISSET(fd, &readfds)) {
      ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
      if (bytes_read < 0) {
        perror("read");
        return -1;
      }
      printf("=> buffer: %s", buffer);
    }
  }
}