#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

int main() {
    const char *serial_port = "/dev/ttyUSB0"; // Change this to your serial port
    int fd = open(serial_port, O_RDONLY | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("Error opening serial port");
        return 1;
    }

    // Configure the serial port (optional, depending on your needs)
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error getting terminal attributes");
        close(fd);
        return 1;
    }

    cfsetospeed(&tty, B9600); // Set baud rate
    cfsetispeed(&tty, B9600);

    tty.c_cflag &= ~PARENB; // No parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8; // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // Disable hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable reading and ignore control lines

    tty.c_lflag &= ~ICANON; // Non-canonical mode
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g., newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    tty.c_cc[VTIME] = 0; // No timeout
    tty.c_cc[VMIN] = 0; // Non-blocking read

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error setting terminal attributes");
        close(fd);
        return 1;
    }

    // Set up the select call
    fd_set read_fds;
    struct timeval timeout;

    // Initialize the set
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    // Set timeout to 5 seconds
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    // Wait for data to be available
    int ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        perror("Error in select");
    } else if (ret == 0) {
        printf("Timeout occurred, no data received.\n");
    } else {
        if (FD_ISSET(fd, &read_fds)) {
            // Data is available, read it
            char buffer[256];
            int n = read(fd, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0'; // Null-terminate the string
                printf("Received: %s\n", buffer);
            } else {
                perror("Error reading from serial port");
            }
        }
    }

    close(fd);
    return 0;
}