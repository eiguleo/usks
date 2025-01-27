#include "./include/queue.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>

#define MAX_SIZE 512
#define SOCKET_PATH "/tmp/u"
#define BUFFER_SIZE 256

SyncQueue enetqueue;

typedef struct fdq {
  int fd;
  SyncQueue *q;
} fdq_t;

// 共享资源
int shared_data = 0;
// 互斥锁
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *unix_server_init() {
  int server_socket, ret;
  struct sockaddr_un server_addr, client_addr;
  socklen_t client_addr_len;
  char buffer[BUFFER_SIZE];

  // Create a datagram (DGRAM) socket in the UNIX domain
  server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (server_socket < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Set up the server address structure
  memset(&server_addr, 0, sizeof(struct sockaddr_un));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

  // Bind the socket to the address
  unlink(SOCKET_PATH);
  ret = bind(server_socket, (struct sockaddr *)&server_addr,
             sizeof(struct sockaddr_un));
  if (ret < 0) {
    perror("bind");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  printf("=> Server is listening on %s\n", SOCKET_PATH);

  while (1) {
    client_addr_len = sizeof(struct sockaddr_un);
    memset(buffer, 0, BUFFER_SIZE);

    // Receive a message from a client
    ret = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0,
                   (struct sockaddr *)&client_addr, &client_addr_len);
    if (ret < 0) {
      perror("recvfrom");
      close(server_socket);
      exit(EXIT_FAILURE);
    }

    // printf("=> Unix dgram recive message: |%s|\n", buffer);
    enqueue(&enetqueue, buffer, sizeof(buffer));
  }

  // Close the socket (unreachable in this example)
  close(server_socket);
  unlink(SOCKET_PATH);

  return 0;
}

void *read_list_write(void *q) {
  fdq_t *fda_p = (fdq_t *)(q);
  int i = 0;
  while (1) {
    // sleep(3);
    QueueElement qel = dequeue(fda_p->q);

    int fd = fda_p->fd;
    char buff[128];
    memset(buff, 0, sizeof(buff));
    strcpy(buff, qel.array);
    // printf("=> Read buff from queue: |%s|\n",buff);

    // printf("=> before write to serial\n");
    int n = write(fd, buff, strlen(buff));
    // printf("=> After write to serial,n: %d\n",n);
  }
}

void *thread_read_serial(void *q) {

  fdq_t *fda_p = (fdq_t *)(q);
  int fd = fda_p->fd;
  char buf[256];
  while (1) {
    int n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
      perror("read");
    } else if (n > 0) {
      buf[n] = '\0';
      // printf("=> Read from serial: \n");
      printf("%s\n", buf);
      // printf("\n");
    }
  }
  return NULL;
}

int set_interface_attribs(int fd, int speed) {
  struct termios tty;
  if (tcgetattr(fd, &tty) != 0) {
    perror("tcgetattr");
    return -1;
  }

  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
  tty.c_iflag &= ~IGNBRK;                     // disable break processing
  tty.c_lflag = 0;                            // no signaling chars, no echo,
                                              // no canonical processing
  tty.c_oflag = 0;                            // no remapping, no delays
  tty.c_cc[VMIN] = 1;  // read blocks until 1 byte is available
  tty.c_cc[VTIME] = 0; // no timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                     // enable reading
  tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
  tty.c_cflag &= ~CSTOPB;
  // tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    perror("tcsetattr");
    return -1;
  }
  return 0;
}

int main() {

  const char *portname = "/dev/ttyUSB0";
  int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  if (set_interface_attribs(fd, B9600) !=
      0) { // set speed to 9600 bps, 8n1 (no parity)
    close(fd);
    return 1;
  }

  init_queue(&enetqueue);
  pthread_t tid1, tid2, tid3;

  fdq_t fdqo;
  fdqo.fd = fd;
  fdqo.q = &enetqueue;

  pthread_create(&tid1, NULL, unix_server_init, NULL);
  pthread_create(&tid2, NULL, read_list_write, (void *)&fdqo);
  pthread_create(&tid3, NULL, thread_read_serial, (void *)&fdqo);

  pthread_join(tid1, NULL);
  pthread_join(tid2, NULL);
  pthread_join(tid3, NULL);

  while (1) {
    printf("=> main loop \n");
    sleep(3);
  }

  return 0;
}