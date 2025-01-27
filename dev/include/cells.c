#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int open_serial_port(const char *port) {
  int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
    perror("open");
    return -1;
  }
  return fd;
}

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

  tty.c_cc[VMIN] = 0;   // 非阻塞读
  tty.c_cc[VTIME] = 10; // 1秒超时

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    perror("tcsetattr");
    return -1;
  }

  return 0;
}

int write_serial_port(int fd, const uint8_t *data, size_t len) {
  ssize_t bytes_written = write(fd, data, len);
  if (bytes_written < 0) {
    perror("write");
    return -1;
  }
  return bytes_written;
}

int read_serial_port(int fd, uint8_t *buffer, size_t len, int timeout_ms) {
  fd_set readfds;
  struct timeval tv;
  int ret;

  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);

  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;




  if (FD_ISSET(fd, &readfds)) {
    ssize_t bytes_read = read(fd, buffer, len);
    if (bytes_read < 0) {
      perror("read");
      return -1;
    }
    return bytes_read;
  }

  return 0;
}

int enet_chat_module(char *usbdev_s, char *at_cmd_s) {
  printf("=> port: %s\n", usbdev_s);
  printf("=> cmd: %s\n", at_cmd_s);
  // const char *port = "/dev/ttyUSB0";
  const char *port = usbdev_s;
  int baud_rate = 115200;
  int fd = open_serial_port(usbdev_s);
  if (fd < 0) {
    return -1;
  }


  if (ioctl(fd, TCFLSH, TCIOFLUSH) < 0) {
      perror("ioctl");
  }



  // const uint8_t write_data[] = "at+cgdcont?\r\n";
  // const uint8_t write_data[] = "at+cgdcont?\r\n";
  const uint8_t write_data1[50];
  strcpy((char *)write_data1, at_cmd_s);

  if (write_serial_port(fd, write_data1, sizeof(write_data1)) < 0) {
    close(fd);
    return -1;
  }

  uint8_t read_buffer[1024];
  for (;;) {
    int bytes_read =
        read_serial_port(fd, read_buffer, sizeof(read_buffer), 5000);
    if (bytes_read < 0) {
      close(fd);
      return -1;
    } else if (bytes_read == 0) {
      printf("Read timeout\n");
    } else {
      read_buffer[bytes_read] = '\0';
      printf("Read: %s\n", read_buffer);
    }
    printf("b s\n");
    sleep(1);
    printf("a s\n");
  }

  close(fd);
}
