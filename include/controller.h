/*
 * Generic controller interface — each controller type implements this.
 * Inspired by MissionControl's EmulatedSwitchController pattern.
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "anycontroller.h"

struct controller;

/* Normalized controller state — every driver maps its raw reports to this,
 * and vpad.c turns it into the PS5 virtual pad report. Sticks use
 * PlayStation convention: 0..255, center 128, Y increasing downwards. */
typedef struct {
    /* Digital buttons */
    bool cross;
    bool circle;
    bool square;
    bool triangle;

    bool l1;
    bool r1;
    bool l3;
    bool r3;

    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;

    bool options;    /* Start / + / Menu */
    bool create;     /* Select / - / View / Share */
    bool ps_button;  /* Home / Guide / PS */
    bool touchpad;   /* Touchpad click (emulated via capture on Switch) */

    /* Analog sticks — normalized to 0..255, center 128 */
    uint8_t left_stick_x;
    uint8_t left_stick_y;
    uint8_t right_stick_x;
    uint8_t right_stick_y;

    /* Analog triggers — 0..255 */
    uint8_t l2_analog;
    uint8_t r2_analog;

    /* Motion — raw sensor values, controller-specific scaling applied */
    bool has_motion;
    float accel_x, accel_y, accel_z;       /* m/s^2 */
    float gyro_x, gyro_y, gyro_z;          /* rad/s */

    /* Orientation quaternion (computed from motion if available) */
    bool has_orientation;
    float orient_x, orient_y, orient_z, orient_w;

    /* Battery */
    uint8_t battery_level;  /* 0..100 */
    bool    charging;

    /* Timestamp */
    uint64_t timestamp_us;
} NormalizedInput;

/* Controller driver vtable — each controller type provides one */
typedef struct controller_driver {
    const char     *name;
    ControllerType  type;

    /* USB identifiers for this controller */
    uint16_t  vendor_id;
    uint16_t  product_id;

    /* Initialize driver state. Returns 0 on success. */
    int  (*init)(struct controller *ctrl);

    /* Optional: talk to the device once its transport is open (handshakes,
     * mode switches, LEDs). Returns 0 on success. */
    int  (*setup)(struct controller *ctrl);

    /* Parse a raw HID report into NormalizedInput.
     * report: raw bytes, len: byte count.
     * Returns 0 on success, -1 if report is not recognized. */
    int  (*parse_input)(struct controller *ctrl,
                        const uint8_t *report, int len,
                        NormalizedInput *out);

    /* Build an output report (rumble, LEDs) to send to the controller.
     * buf: output buffer, max_len: buffer size.
     * Returns bytes written, or 0 if no output needed. */
    int  (*build_output)(struct controller *ctrl,
                         uint8_t large_motor, uint8_t small_motor,
                         uint8_t r, uint8_t g, uint8_t b,
                         uint8_t *buf, int max_len);

    /* Clean up driver resources */
    void (*cleanup)(struct controller *ctrl);
} ControllerDriver;

/* Controller instance */
typedef struct controller {
    const ControllerDriver *driver;
    ControllerState         state;
    TransportType           transport;
    int                     slot;        /* 0..MAX_CONTROLLERS-1 */

    /* Transport handle (USB fd or BT socket) */
    int                     fd;

    /* Latest parsed input */
    NormalizedInput         input;

    /* Driver-private data */
    void                   *priv;
} Controller;

/* --- Driver registry --- */

/* Register a driver. Called at startup. */
void controller_driver_register(const ControllerDriver *drv);

/* Find a driver by USB VID/PID */
const ControllerDriver *controller_driver_find(uint16_t vid, uint16_t pid);

/* Find a driver by type */
const ControllerDriver *controller_driver_find_by_type(ControllerType type);

/* Initialize the driver registry with all built-in drivers */
void controller_drivers_init(void);

#endif /* CONTROLLER_H */
