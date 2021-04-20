#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int enable_and_reset_display(int port) {
  uint8_t buf[2];
  ssize_t bytes_written;

  fprintf(stderr, "Enabling and resetting M8 display\n");

  buf[0] = 0x44;
  bytes_written = write(port, buf, 1);
  if (bytes_written == -1) {
    fprintf(stderr, "Error code %d: %s\n", errno, strerror(errno));
    return -1;
  }
  usleep(500);
  buf[0] = 0x45;
  buf[1] = 0x52;
  bytes_written = write(port, buf, 2);
  if (bytes_written == -1) {
    fprintf(stderr, "Error code %d: %s\n", errno, strerror(errno));
    return -1;
  }
  sleep(1);

  return 1;
}

int disconnect(int port) {
  char buf[1] = {'D'};
  size_t nbytes = 1;
  ssize_t bytes_written;

  fprintf(stderr, "Disconnecting M8\n");

  bytes_written = write(port, buf, nbytes);
  if (bytes_written != nbytes) {
    fprintf(stderr,
            "Error disconnecting, expected to write %zu bytes, %zd written\n",
            nbytes, bytes_written);

    if (bytes_written == -1) {
      fprintf(stderr, "Error code %d: %s\n", errno, strerror(errno));
    }
    return -1;
  }
  return 1;
}

int send_msg_controller(int port, uint8_t input) {
  char buf[2] = {'C',input};
  size_t nbytes = 2;
  ssize_t bytes_written;
  bytes_written = write(port, buf, nbytes);
  if (bytes_written != nbytes) {
    fprintf(stderr,
            "Error sending controller message, expected to write %zu bytes, %zd written\n",
            nbytes, bytes_written);

    if (bytes_written == -1) {
      fprintf(stderr, "Error code %d: %s\n", errno, strerror(errno));
    }
    return -1;
  }
  return 1;

}

int send_msg_keyjazz(int port, uint8_t note, uint8_t velocity) {
  if (velocity > 64)
    velocity = 64;
  char buf[3] = {'K',note,velocity};
  size_t nbytes = 3;
  ssize_t bytes_written;
  bytes_written = write(port, buf, nbytes);
  if (bytes_written != nbytes) {
    fprintf(stderr,
            "Error sending keyjazz message, expected to write %zu bytes, %zd written\n",
            nbytes, bytes_written);

    if (bytes_written == -1) {
      fprintf(stderr, "Error code %d: %s\n", errno, strerror(errno));
    }
    return -1;
  }
  return 1;

}