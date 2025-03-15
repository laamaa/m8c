#ifndef M8C_USB_H_
#define M8C_USB_H_
#ifdef USE_LIBUSB

#include "slip.h"
#include <libusb.h>

extern libusb_device_handle *devh;
int async_read(uint8_t *serial_buf, int count, slip_handler_s *slip);

#endif // USE_LIBUSB
#endif // M8C_USB_H_
