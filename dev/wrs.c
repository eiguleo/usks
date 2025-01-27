#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// Function to configure the serial port
int configure_serial_port(int fd) {
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }

    // Set Baud Rate
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    // Setting other Port Stuff
    tty.c_cflag &= ~PARENB;  // Make 8n1
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    tty.c_cflag &= ~CRTSCTS;  // No flow control
    tty.c_cc[VMIN] = 1;       // Read doesn't block
    tty.c_cc[VTIME] = 5;      // 0.5 seconds read timeout

    tty.c_cflag |= CREAD | CLOCAL;  // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    // Make raw
    cfmakeraw(&tty);

    // Flush Port, then applies attributes
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

// Function to write data to the serial port
int serial_write(int fd, const char *data) {}

// Function to read data from the serial port


int main() {
    const char *portname = "/dev/ttyUSB0";  // Change this to your serial port
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (configure_serial_port(fd) != 0) {
        close(fd);
        return -1;
    }

    // Write to the serial port
    // char data[] = "ati\r\n";
    char *data = "ati\r\n";
    int len;

    len = write(fd, data, strlen(data));
    if (len < 0) {
        perror("write");
    }
    // Read from the serial port
    while (1) {
        char buffer[100];
        memset(buffer, 0, sizeof(buffer));
        len = serial_read(fd, buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';  // Null-terminate the string
            printf("Received: %s\n", buffer);
        }
        sleep(1);
    }

    close(fd);
    return 0;
}