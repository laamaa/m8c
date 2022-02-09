// Copyright 2021 Jonne Kokkonen
// Released under the MIT licence, https://opensource.org/licenses/MIT

#include <SDL2/SDL.h>
#include <libserialport.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "config.h"
#include "input.h"
#include "render.h"
// #include "serial.h"
#include "slip.h"
#include "write.h"
#include <enet/enet.h>

// maximum amount of bytes to read from the serial in one read()
#define serial_read_size 10368

uint8_t run = 1;
uint8_t need_display_reset = 0;

// Handles CTRL+C / SIGINT
void intHandler(int dummy) { run = 0; }

ENetAddress address;
ENetHost * client;
ENetPeer * peer;

int main(int argc, char *argv[]) {



  // Initialize the config to defaults read in the params from the
  // configfile if present
  config_params_s conf = init_config();

  // TODO: take cli parameter to override default configfile location
  read_config(&conf);



  if (enet_initialize () != 0)
  {
      fprintf (stderr, "An error occurred while initializing ENet.\n");
      return EXIT_FAILURE;
  }
  atexit (enet_deinitialize);



  client = enet_host_create (NULL /* create a client host */,
              1 /* only allow 1 outgoing connection */,
              3 /* allow up 3 channels to be used, 0, 1 and 2 */,
              0 /* assume any amount of incoming bandwidth */,
              0 /* assume any amount of outgoing bandwidth */);

  if (client == NULL)
  {
      fprintf (stderr, 
              "An error occurred while trying to create an ENet server host.\n");
      exit (EXIT_FAILURE);
  }
  ENetEvent event;


  /* Bind the server to port 1234. */
  //enet_address_set_host (& address, "127.0.0.1");
  enet_address_set_host (& address, "192.168.1.146");
  address.port = 1234;

  /* Initiate the connection, allocating the two channels 0 and 1. */
  peer = enet_host_connect (client, & address, 2, 0);   


  if (peer == NULL)
  {
    fprintf (stderr, 
              "No available peers for initiating an ENet connection.\n");
    exit (EXIT_FAILURE);
  }
  /* Wait up to 5 seconds for the connection attempt to succeed. */
  if (enet_host_service (client, & event, 5000) > 0 &&
      event.type == ENET_EVENT_TYPE_CONNECT)
  {
      SDL_LogInfo(0, "Connection to 192.168.1.146:1234 succeeded");
  }
  else
  {
      /* Either the 5 seconds are up or a disconnect event was */
      /* received. Reset the peer in the event the 5 seconds   */
      /* had run out without any significant event.            */
      enet_peer_reset (peer);
      SDL_LogInfo(0, "Connection to some.server.net:1234 failed.");
      return 0;
  }

  SDL_LogInfo(0, "After Client init");
  // allocate memory for serial buffer
  uint8_t *serial_buf = malloc(serial_read_size);

  static uint8_t slip_buffer[serial_read_size]; // SLIP command buffer

  // settings for the slip packet handler
  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = process_command, // the function where complete slip
                                       // packets are processed further
  };

  static slip_handler_s slip;

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  slip_init(&slip, &slip_descriptor);


  if (initialize_sdl(conf.init_fullscreen) == -1)
    run = 0;

  uint8_t prev_input = 0;

  // main loop
  while (run) {
    //SDL_LogInfo(0, "Run Loop");
    enet_host_service(client, & event, 0);

    switch (event.type)
    {
      case ENET_EVENT_TYPE_NONE:
        break;
      case ENET_EVENT_TYPE_CONNECT:
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        run=0;
        printf("You have been disconnected.\n");
    }


    
    size_t bytes_read = 0;

    if (event.peer && event.type == ENET_EVENT_TYPE_RECEIVE event.channelID == 2) {
      serial_buf = event.packet -> data;
      bytes_read = event.packet -> dataLength;
      peer = event.peer;
    }
   
    if (serial_buf) {

      for (int i = 0; i < bytes_read; i++) {
        uint8_t rx = serial_buf[i];
        // process the incoming bytes into commands and draw them
        int n = slip_read_byte(&slip, rx);
        if (n != SLIP_NO_ERROR) {
          if (n == SLIP_ERROR_INVALID_PACKET) {
            //reset_display(port);
          } else {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SLIP error %d\n", n);
          }
        }
      }
      // don't need to render in the server 
      render_screen(); // TODO comment out
      serial_buf = 0;
    }

    //SDL_LogInfo(0, "After enet host");
    // get current inputs
    input_msg_s input = get_input_msg(&conf);

    switch (input.type) {
    case normal:
      if (input.value != prev_input) {
        prev_input = input.value;
        if (peer) {
          send_msg_controller_client(peer, input.value);
        }
      }
      break;
    case keyjazz:
      if (input.value != prev_input) {
        prev_input = input.value;
        if (peer) {
          if (input.value != 0) {
            send_msg_keyjazz_client(peer, input.value, 0xFF);
          } else {
            send_msg_keyjazz_client(peer, 0, 0);
          }
        }
      }
      break;
    case special:
      if (input.value != prev_input) {
        prev_input = input.value;
        switch (input.value) {
        case msg_quit:
          run = 0;
          break;
        case msg_reset_display:
          break;
        default:
          break;
        }
        break;
      }
    }
    // if (peer) {
    //   enet_host_flush(peer);
    // }

  }

  // exit, clean up
  SDL_Log("Shutting down\n");
  close_game_controllers();
  close_renderer();
  // disconnect(port);
  // sp_close(port);
  // sp_free_port(port);
  free(serial_buf);
  enet_host_destroy(client);
  return 0;

}
