/*
 * Xbox 360 wired (XInput) protocol — also spoken by many third-party pads and
 * 2.4GHz dongles in "X-input" mode (e.g. GameSir T4 Pro), which report 045e:028e.
 */

#ifndef XBOX360_H
#define XBOX360_H

#include "../controller.h"

#define XBOX_VID           0x045E
#define XBOX360_PID        0x028E

/* Input message: type 0x00, length 0x14 */
#define X360_MSG_INPUT     0x00
#define X360_INPUT_LEN     0x14

/* Buttons, little-endian u16 at bytes 2..3 */
#define X360_DPAD_UP       0x0001
#define X360_DPAD_DOWN     0x0002
#define X360_DPAD_LEFT     0x0004
#define X360_DPAD_RIGHT    0x0008
#define X360_START         0x0010
#define X360_BACK          0x0020
#define X360_LSTICK        0x0040
#define X360_RSTICK        0x0080
#define X360_LB            0x0100
#define X360_RB            0x0200
#define X360_GUIDE         0x0400
#define X360_A             0x1000
#define X360_B             0x2000
#define X360_X             0x4000
#define X360_Y             0x8000

const ControllerDriver *xbox360_get_driver(void);

#endif /* XBOX360_H */
