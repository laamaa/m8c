// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL_log.h>
#include <libserialport.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <enet/enet.h>

uint8_t prev_input = 0;

int reset_display(struct sp_port *port){
  SDL_Log("Reset display\n");
  uint8_t buf[2];
  int result;

  buf[0] = 0x45;
  buf[1] = 0x52;
    
  result = sp_blocking_write(port, buf, 2, 5);
  if (result != 2) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display, code %d", result);
    return 0;
  }
  return 1;
}

int enable_and_reset_display(struct sp_port *port) {
  uint8_t buf[1];
  int result;

  SDL_Log("Enabling and resetting M8 display\n");

  buf[0] = 0x44;
  result = sp_blocking_write(port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display, code %d", result);
    return 0;
  }

  usleep(500);
  if (!reset_display(port)){
    return 0;
  }
  return 1;
}

int disconnect(struct sp_port *port) {
  char buf[1] = {'D'};
  int result;

  SDL_Log("Disconnecting M8\n");

  result = sp_blocking_write(port, buf, 1, 5);
  if (result != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending disconnect, code %d", result);
    return -1;
  }
  return 1;
}

int send_msg_controller(struct sp_port *port, uint8_t input) {
  char buf[2] = {'C',input};
  size_t nbytes = 2;
  int result;
  result = sp_blocking_write(port, buf, nbytes, 5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending input, code %d", result);
    return -1;
  }
  return 1;

}

int send_msg_keyjazz(struct sp_port *port, uint8_t note, uint8_t velocity) {
  if (velocity > 64)
    velocity = 64;
  char buf[3] = {'K',note,velocity};
  size_t nbytes = 3;
  int result;
  result = sp_blocking_write(port, buf, nbytes,5);
  if (result != nbytes) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending keyjazz, code %d", result);
    return -1;
  }

  return 1;
}



int send_msg_controller_server(struct sp_port *port, unsigned char buf[2]) {
  size_t nbytes = 2;
  if (buf[1] != prev_input) {
    prev_input = buf[1];
    sp_blocking_write(port, buf, nbytes, 5);
  }
  return 1;
}

int send_msg_keyjazz_server(struct sp_port *port, unsigned char buf[3]) {
  size_t nbytes = 3;
  if (buf[1] != prev_input) {
    prev_input = buf[1];
    sp_blocking_write(port, buf, nbytes,5);
  }
  return 1;
}

void send_msg_controller_client(ENetPeer * peer, uint8_t input) {
  char buf[2] = {'C',input};
  size_t nbytes = 2;
  ENetPacket * packet = enet_packet_create (buf, 
                                            nbytes);
  enet_peer_send (peer, 0, packet);
  //enet_host_flush(peer);
}

void send_msg_keyjazz_client(ENetPeer * peer, uint8_t note, uint8_t velocity) {
  if (velocity > 64)
    velocity = 64;
  char buf[3] = {'K',note,velocity};
  size_t nbytes = 3;
  ENetPacket * packet = enet_packet_create (buf, 
                                            nbytes);
  enet_peer_send (peer, 1, packet);
  //enet_host_flush(peer);
}
