// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#ifndef WRITE_H_
#define WRITE_H_

#include <stdint.h>
#include <libserialport.h>
#include <enet/enet.h>

int reset_display(struct sp_port *port);
int enable_and_reset_display(struct sp_port *port);
int disconnect(struct sp_port *port);
int send_msg_controller(struct sp_port *port, uint8_t input);
int send_msg_keyjazz(struct sp_port *port, uint8_t note, uint8_t velocity);
int send_msg_controller_server(struct sp_port *port, unsigned char buf[2]);
int send_msg_keyjazz_server(struct sp_port *port, unsigned char buf[3]);
void send_msg_controller_client(ENetPeer * peer, uint8_t input);
void send_msg_keyjazz_client(ENetPeer * peer, uint8_t note, uint8_t velocity);
#endif