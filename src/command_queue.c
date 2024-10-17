#include <SDL.h>

const int command_size = 4096;
int write_cursor = 0;
int read_cursor = 0;
uint8_t **command_buffer;
uint32_t *command_sizes;

void command_queue_init() {
  command_buffer = SDL_malloc(command_size * sizeof(uint8_t *));
  command_sizes = SDL_malloc(command_size * sizeof(uint32_t));
}

void command_queue_destroy() {
  SDL_free(command_buffer);
  SDL_free(command_sizes);
}

int command_queue_push(uint8_t *data, uint32_t size) {
  uint8_t *recv_buf = SDL_malloc(size * sizeof(uint8_t));
  SDL_memcpy(recv_buf, data, size);

  SDL_free(command_buffer[write_cursor % command_size]);
  command_buffer[write_cursor % command_size] = recv_buf;
  command_sizes[write_cursor % command_size] = size;
  write_cursor++;
  return 1;
}

int command_queue_pop(uint8_t **data, uint32_t *size) {
  int compare = write_cursor - read_cursor;
  if (compare == 0) {
    return 0;
  }
  *data = command_buffer[read_cursor % command_size];
  *size = command_sizes[read_cursor % command_size];
  read_cursor++;
  return compare;
}
