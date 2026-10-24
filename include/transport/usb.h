/*
 * USB transport — the PS5 exposes USB devices only through FreeBSD's ugen
 * driver (/dev/ugenB.A, /dev/usb/B.A.E). There is no uhid. Devices are
 * identified with USB_GET_DEVICE_DESC on the control node, Sony's own HID
 * driver is detached, and reports are read/written on the endpoint node.
 */

#ifndef TRANSPORT_USB_H
#define TRANSPORT_USB_H

#include "../anycontroller.h"
#include "../controller.h"
#include <stdint.h>

#define USB_MAX_DEVICES     MAX_CONTROLLERS
#define USB_MAX_ENDPOINT    15

typedef struct {
    int       ctrl_fd;      /* /dev/usb/B.A.0 — held to keep the interface claimed */
    int       in_fd;        /* first interrupt IN endpoint — reports from the pad */
    int       out_fd;       /* first OUT endpoint — commands to the pad, or -1 */
    uint16_t  vendor_id;
    uint16_t  product_id;
    char      name[16];     /* "B.A" */
    bool      active;
} UsbDevice;

/* Find one supported controller that isn't already open and fill in
 * ctrl->driver / fd / transport. Does not call driver->init.
 * Returns 0 if a controller was opened, -1 if none is available. */
int usb_open_next_controller(Controller *ctrl);

/* Read one raw report. Returns bytes read, 0 if none pending, -1 on
 * disconnect/error. Non-blocking. */
int usb_read_report(int fd, uint8_t *buf, int max_len);

/* Write one raw report to the OUT endpoint of the device whose IN endpoint
 * is fd (ctrl->fd). Returns bytes written, or -1 on error. */
int usb_write_report(int fd, const uint8_t *buf, int len);

/* Close a controller opened by usb_open_next_controller (by its fd). */
void usb_close_device(int fd);

int  usb_init(void);
void usb_shutdown(void);

#endif /* TRANSPORT_USB_H */
