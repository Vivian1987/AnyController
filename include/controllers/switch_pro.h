/*
 * Nintendo Switch Pro Controller driver.
 *
 * HID report format reference:
 *   https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
 *
 * The Pro Controller uses report ID 0x30 for full input reports (buttons,
 * sticks, IMU) and 0x21 for subcommand replies. Sticks are 12-bit per axis
 * packed into 3 bytes each. IMU delivers 3 samples of 6-axis data per report.
 */

#ifndef SWITCH_PRO_H
#define SWITCH_PRO_H

#include "../controller.h"

#define SWITCH_VID                0x057E
#define SWITCH_PRO_PID            0x2009
#define SWITCH_JOYCON_L_PID       0x2006
#define SWITCH_JOYCON_R_PID       0x2007

/* HID report IDs */
#define SWITCH_REPORT_INPUT_FULL  0x30  /* Standard full report */
#define SWITCH_REPORT_INPUT_SIMPLE 0x3F /* Simple HID report (USB only) */
#define SWITCH_REPORT_SUBCMD_REPLY 0x21
#define SWITCH_REPORT_OUTPUT      0x01  /* Rumble + subcommand */
#define SWITCH_REPORT_RUMBLE_ONLY 0x10

/* Subcommand IDs */
#define SWITCH_SUBCMD_SET_INPUT_MODE   0x03
#define SWITCH_SUBCMD_SET_PLAYER_LEDS  0x30
#define SWITCH_SUBCMD_ENABLE_IMU       0x40
#define SWITCH_SUBCMD_ENABLE_VIBRATION 0x48

/* Button bitmask — byte 0 of button data (right side) */
#define SWITCH_BTN_Y         0x01
#define SWITCH_BTN_X         0x02
#define SWITCH_BTN_B         0x04
#define SWITCH_BTN_A         0x08
#define SWITCH_BTN_SR_R      0x10
#define SWITCH_BTN_SL_R      0x20
#define SWITCH_BTN_R         0x40
#define SWITCH_BTN_ZR        0x80

/* Button bitmask — byte 1 (shared) */
#define SWITCH_BTN_MINUS     0x01
#define SWITCH_BTN_PLUS      0x04
#define SWITCH_BTN_RSTICK    0x08
#define SWITCH_BTN_LSTICK    0x10
#define SWITCH_BTN_HOME      0x20
#define SWITCH_BTN_CAPTURE   0x40

/* Button bitmask — byte 2 (left side) */
#define SWITCH_BTN_DPAD_DOWN  0x01
#define SWITCH_BTN_DPAD_UP    0x02
#define SWITCH_BTN_DPAD_RIGHT 0x04
#define SWITCH_BTN_DPAD_LEFT  0x08
#define SWITCH_BTN_SR_L       0x10
#define SWITCH_BTN_SL_L       0x20
#define SWITCH_BTN_L          0x40
#define SWITCH_BTN_ZL         0x80

/* Pro Controller stick calibration defaults (center, min, max) */
#define SWITCH_STICK_CENTER  2048
#define SWITCH_STICK_MIN     512
#define SWITCH_STICK_MAX     3584

/* IMU sensitivity defaults */
#define SWITCH_ACCEL_SCALE   (8192.0f)   /* LSB per g (±4g range) */
#define SWITCH_GYRO_SCALE    (13371.0f)  /* LSB per dps (±2000dps range) */

/* Raw 0x30 report layout */
typedef struct __attribute__((packed)) {
    uint8_t  report_id;     /* 0x30 */
    uint8_t  timer;
    uint8_t  battery_conn;  /* [7:4] battery, [3:0] conn info */
    uint8_t  buttons[3];    /* right, shared, left */
    uint8_t  left_stick[3]; /* 12-bit X in lower, 12-bit Y in upper */
    uint8_t  right_stick[3];
    uint8_t  vibrator;
    /* 6-axis IMU: 3 samples × (3 accel + 3 gyro) × int16 = 36 bytes */
    int16_t  imu[3][6];     /* [sample][accel_x,y,z, gyro_x,y,z] */
} SwitchFullReport;

/* Driver-private state */
typedef struct {
    uint8_t  output_counter;  /* Incrementing counter for output reports */
    bool     imu_enabled;
    bool     vibration_enabled;

    /* Stick calibration */
    struct {
        uint16_t center_x, center_y;
        uint16_t min_x, min_y;
        uint16_t max_x, max_y;
    } cal_left, cal_right;
} SwitchProPrivate;

/* Get the driver vtable */
const ControllerDriver *switch_pro_get_driver(void);

/* Run the initialization sequence (enable IMU, full reports, LEDs) */
int switch_pro_setup(Controller *ctrl);

#endif /* SWITCH_PRO_H */
