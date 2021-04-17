#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int set_interface_attribs(int fd, int speed, int parity) {
  struct termios tty;
  if (tcgetattr(fd, &tty) != 0) {
    fprintf(stderr, "error %d from tcgetattr", errno);
    return -1;
  }

  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
  // disable IGNBRK for mismatched speed tests; otherwise receive break
  // as \000 chars
  tty.c_iflag &= ~IGNBRK; // disable break processing
  tty.c_lflag = 0;        // no signaling chars, no echo,
                          // no canonical processing
  tty.c_oflag = 0;        // no remapping, no delays
  tty.c_cc[VMIN] = 0;     // read doesn't block
  tty.c_cc[VTIME] = 5;    // 0.5 seconds read timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                     // enable reading
  tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
  tty.c_cflag |= parity;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    fprintf(stderr, "Error %d from tcsetattr\n", errno);
    return -1;
  }
  return 0;
}

static void set_blocking(int fd, int should_block) {

  struct termios tty;
  memset(&tty, 0, sizeof tty);
  if (tcgetattr(fd, &tty) != 0) {
    fprintf(stderr, "Error %d from tggetattr\n", errno);
    return;
  }

  tty.c_cc[VMIN] = should_block ? 1 : 0;
  tty.c_cc[VTIME] = should_block ? 5 : 0;

  if (tcsetattr(fd, TCSANOW, &tty) != 0)
    fprintf(stderr, "Error %d setting term attributes\n", errno);
}

int init_serial(char *portname) {

  int fd = open(portname, O_RDWR);

  if (fd < 0) {
    fprintf(stderr, "Error %d opening %s: %s\n", errno, portname,
            strerror(errno));
    return -1;
  }

  set_interface_attribs(fd, __MAX_BAUD,
                        0); // set speed to max bps, 8n1 (no parity)
  set_blocking(fd, 0);      // set no blocking

  return fd;
}