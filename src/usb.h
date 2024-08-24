#ifndef M8C_USB_H_
#define M8C_USB_H_
#ifdef USE_LIBUSB

#include <libusb.h>
extern libusb_device_handle *devh;

#endif // USE_LIBUSB
#endif // M8C_USB_H_
