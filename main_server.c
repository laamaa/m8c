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
#include "serial.h"
#include "slip.h"
#include "write.h"
#include <enet/enet.h>

// maximum amount of bytes to read from the serial in one read()
#define serial_read_size 324

uint8_t run = 1;  
int main_loop=1;
uint8_t need_display_reset = 0;

// Handles CTRL+C / SIGINT
void intHandler(int dummy) { 
  run = 0; 
  main_loop = 0;
}


ENetAddress address;
ENetHost * server;


int main(int argc, char *argv[]) {


  struct sp_port *port;




  if (enet_initialize () != 0)
  {
      fprintf (stderr, "An error occurred while initializing ENet.\n");
      return EXIT_FAILURE;
  }
  atexit (enet_deinitialize);

  enet_address_set_host (& address, "192.168.1.146");
  //enet_address_set_host (& address, "127.0.0.1");
  /* Bind the server to port 1234. */
  address.port = 1234;


  ENetEvent event;

  while (main_loop) {
    //SDL_Log("Creating Server.\n");
    server = enet_host_create (& address /* the address to bind the server host to */, 
                              1      /* allow up to 32 clients and/or outgoing connections */,
                                2      /* allow up to 2 channels to be used, 0 and 1 */,
                                0      /* assume any amount of incoming bandwidth */,
                                0      /* assume any amount of outgoing bandwidth */);
    if (server == NULL)
    {
        fprintf (stderr, 
                "An error occurred while trying to create an ENet server host.\n");
        exit (EXIT_FAILURE);
    }

    int connected = 0;
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



    // Initialize the config to defaults read in the params from the
    // configfile if present
    config_params_s conf = init_config();

    // TODO: take cli parameter to override default configfile location
    read_config(&conf);




    signal(SIGINT, intHandler);
    signal(SIGTERM, intHandler);

    slip_init(&slip, &slip_descriptor);



    port = init_serial();
    if (port == NULL)
      return -1;

    if (enable_and_reset_display(port) == -1)
      run = 0;
    SDL_Log("After reset display");


    // main loop
    while (run) {

      enet_host_service(server, & event, 0);
      


      switch (event.type)
      {
        case ENET_EVENT_TYPE_NONE:
          break;
        case ENET_EVENT_TYPE_CONNECT:
          connected = 1;
          break;
        case ENET_EVENT_TYPE_RECEIVE:
          break;
        case ENET_EVENT_TYPE_DISCONNECT:
          connected = 0;
          run=0;
          SDL_Log("You have been disconnected.\n");
      }

      if (connected == 1) {



        if (event.peer && event.type == ENET_EVENT_TYPE_RECEIVE && event.channelID == 0) {
          send_msg_controller_server(port, event.packet -> data);
        }

        if (event.peer && event.type == ENET_EVENT_TYPE_RECEIVE && event.channelID == 1) {
          send_msg_keyjazz_server(port, event.packet -> data);
        }


        // read serial port
        size_t bytes_read = sp_blocking_read(port, serial_buf, serial_read_size, 3);
        if (bytes_read < 0) {
          SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error %d reading serial. \n",
                          (int)bytes_read);
          run = 0;
        }
        if (bytes_read > 0) {
          // Send the bytes to the client
          ENetPacket * packet = enet_packet_create (serial_buf, 
                                                    bytes_read, 
                                                    ENET_PACKET_FLAG_RELIABLE);
          

          for (int i = 0; i < bytes_read; i++) {
            uint8_t rx = serial_buf[i];

            // process the incoming bytes into commands and draw them
            int n = slip_read_byte(&slip, rx);
            if (n != SLIP_NO_ERROR) {
              if (n == SLIP_ERROR_INVALID_PACKET) {
                reset_display(port);
              } else {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SLIP error %d\n", n);
              }
            }
          }
          enet_host_broadcast(server, 0, packet);
          enet_host_flush(server);
        }
      }
    
    }

    // exit, clean up
    SDL_Log("Shutting down\n");
    close_game_controllers();
    close_renderer();
    disconnect(port);
    sp_close(port);
    sp_free_port(port);
    free(serial_buf);
    enet_host_destroy(server);
    run=1;
    need_display_reset = 1;
  }

  return 0;
}
