#ifndef M8C_USB_H_
#define M8C_USB_H_
#ifdef USE_LIBUSB

#include <libusb.h>
#include "../config.h"

extern libusb_device_handle *devh;
int init_serial_with_file_descriptor(int file_descriptor);
int m8_initialize(int verbose, const char *preferred_device);
int m8_list_devices(void);
int m8_reset_display(void);
int m8_enable_and_reset_display(void);
int m8_send_msg_controller(uint8_t input);
int m8_send_msg_keyjazz(uint8_t note, uint8_t velocity);
int m8_process_data(config_params_s conf);
int m8_close(void);
#endif // USE_LIBUSB
#endif // M8C_USB_H_
