#ifndef M8C_USB_H_
#define M8C_USB_H_
#ifdef USE_LIBUSB

#include <libusb.h>
extern libusb_device_handle *devh;
int init_serial_with_file_descriptor(int file_descriptor);
#endif // USE_LIBUSB
#endif // M8C_USB_H_
