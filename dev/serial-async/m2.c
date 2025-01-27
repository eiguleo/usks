#include <errno.h> // Error number definitions
#include <fcntl.h> // File control definitions
#include <pthread.h>
#include <stdio.h>
#include <string.h>     // String function definitions
#include <sys/select.h> // For select()
#include <termios.h>    // POSIX terminal control definitions
#include <unistd.h>     // UNIX standard function definitions
int configure_serial_port(int fd) {
  struct termios tty;
  memset(&tty, 0, sizeof(tty));

  // Get current settings
  if (tcgetattr(fd, &tty) != 0) {
    perror("Error getting terminal attributes");
    return -1;
  }

  // Set baud rate (input and output)
  cfsetispeed(&tty, B115200); // Input baud rate
  cfsetospeed(&tty, B115200); // Output baud rate

  // Set other serial port settings
  tty.c_cflag &= ~PARENB;        // No parity
  tty.c_cflag &= ~CSTOPB;        // 1 stop bit
  tty.c_cflag &= ~CSIZE;         // Clear data size bits
  tty.c_cflag |= CS8;            // 8 data bits
  tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
  tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

  tty.c_lflag &= ~ICANON; // Disable canonical mode (raw input)
  tty.c_lflag &= ~ECHO;   // Disable echo 
  tty.c_lflag &= ~ECHOE;  // Disable erasure
  tty.c_lflag &= ~ECHONL; // Disable new-line echo
  tty.c_lflag &= ~ISIG;   // Disable interpretation of INTR, QUIT, and SUSP

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

  tty.c_oflag &= ~OPOST; // Disable output processing
  tty.c_oflag &= ~ONLCR; // Disable newline conversion

  // Set the new settings
  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    perror("Error setting terminal attributes");
    return -1;
  }

  return 0;
}

void *thread_w(void *fd_t) {
  int fd = *(int *)fd_t;
  while (1) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    // Set timeout for select()
    struct timeval timeout;
    timeout.tv_sec = 3; // 1 second timeout
    timeout.tv_usec = 0;

    // Wait for activity on the serial port
    // printf("=> w before select\n");
    int ret = select(fd + 1, &read_fds, NULL, NULL, NULL);
    // printf("=> w after select\n");
    if (ret == -1) {
      perror("=> w: Error in select()");
      break;
    } else if (ret == 0) {
      // Timeout occurred (no data available)
      printf("=> w: Timeout, no data received\n");
      continue;
    }
    // Example: Write data to the serial port
    const char *msg = "Hello, Serial!\r\n";
    int n = write(fd, msg, strlen(msg));
    if (n < 0) {
      perror("=> w: Error writing to serial port");
      break;
    }
    // printf("=> w: sleep 2\n");
    // sleep(2);
  }

  return NULL;
}
void *thread_r(void *fd_t) {
  int fd = *(int *)fd_t;
  while (1) {
    fd_set read_fds;
    FD_ZERO(&read_fds);

    FD_SET(fd, &read_fds);

    // Set timeout for select()
    struct timeval timeout;
    timeout.tv_sec = 3; // 1 second timeout
    timeout.tv_usec = 0;

    // Wait for activity on the serial port
    // printf("=> r before select\n");    
    int ret = select(fd + 1, &read_fds, NULL, NULL, NULL);
    // printf("=> r after select\n");
    if (ret == -1) {
      perror("Error in select()");
      break;
    } else if (ret == 0) {
      // Timeout occurred (no data available)
      printf("=> r: Timeout, no data received\n");
      continue;
    }

    // Check if the serial port is ready for reading
    if (FD_ISSET(fd, &read_fds)) {
      char buf[256];
      int n = read(fd, buf, sizeof(buf));
      if (n < 0) {
        perror("Error reading from serial port");
        break;
      } else if (n == 0) {
        printf("=> r: No data received\n");
      } else {
        buf[n] = '\0'; // Null-terminate the string
        printf("=> r: Received: %s\n", buf);
      }
    }
    // sleep(2);
    // printf("=> r: sleep 2\n");
  }

  return NULL;
}

int main() {
  const char *portname = "/tmp/t1"; // Replace with your serial port
  int fd;

  // Open the serial port (read/write, no controlling terminal, non-blocking)
  fd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd == -1) {
    perror("Error opening serial port");
    return 1;
  }

  // Configure the serial port
  if (configure_serial_port(fd) != 0) {
    close(fd);
    return 1;
  }

  pthread_t tid1, tid2;

  // 创建两个线程
  pthread_create(&tid1, NULL, thread_w, &fd);
  pthread_create(&tid2, NULL, thread_r, &fd);

  // 等待线程结束
  pthread_join(tid1, NULL);
  pthread_join(tid2, NULL);

  // 输出共享资源的最终值

  // 销毁互斥锁
  close(fd);
  return 0;
}