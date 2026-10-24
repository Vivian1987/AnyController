/*
 * AnyController — Use any gamepad on jailbroken PS5
 * Target: PS5 FW 13.60 (FreeBSD-based)
 */

#ifndef ANYCONTROLLER_H
#define ANYCONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#define ANYCONTROLLER_VERSION_MAJOR 0
#define ANYCONTROLLER_VERSION_MINOR 1
#define ANYCONTROLLER_VERSION_PATCH 0

#define POLL_INTERVAL_US  8333  /* ~120Hz */
#define MAX_CONTROLLERS   4

typedef enum {
    CTRL_TYPE_UNKNOWN = 0,
    CTRL_TYPE_SWITCH_PRO,
    CTRL_TYPE_SWITCH_JOYCON_L,
    CTRL_TYPE_SWITCH_JOYCON_R,
    CTRL_TYPE_SWITCH_JOYCON_DUAL,
    CTRL_TYPE_XBOX_ONE,
    CTRL_TYPE_XBOX_SERIES,
    CTRL_TYPE_XBOX_360,
    CTRL_TYPE_DS3,
    CTRL_TYPE_DS4,
    CTRL_TYPE_GENERIC_HID,
} ControllerType;

typedef enum {
    TRANSPORT_NONE = 0,
    TRANSPORT_USB,
    TRANSPORT_BLUETOOTH,
} TransportType;

typedef enum {
    CTRL_STATE_DISCONNECTED = 0,
    CTRL_STATE_CONNECTING,
    CTRL_STATE_CONNECTED,
    CTRL_STATE_ERROR,
} ControllerState;

#endif /* ANYCONTROLLER_H */
