/*
 * Switch Pro Controller driver — parses raw HID reports and maps
 * buttons/sticks/motion to NormalizedInput.
 */

#include "../../include/controllers/switch_pro.h"
#include "../../include/transport/usb.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <ps5/klog.h>

/* Decode 12-bit stick from 3-byte packed format */
static void decode_stick(const uint8_t raw[3], uint16_t *x, uint16_t *y)
{
    *x = raw[0] | ((raw[1] & 0x0F) << 8);
    *y = (raw[1] >> 4) | (raw[2] << 4);
}

/* Scale a 12-bit stick value to 0..255 using calibration */
static uint8_t scale_stick_axis(uint16_t raw, uint16_t center,
                                uint16_t min, uint16_t max)
{
    float normalized;

    if (raw < min) raw = min;
    if (raw > max) raw = max;

    if (raw < center) {
        if (center == min) return 128;
        normalized = (float)(raw - min) / (float)(center - min);
        return (uint8_t)(normalized * 128.0f);
    } else {
        if (max == center) return 128;
        normalized = (float)(raw - center) / (float)(max - center);
        return 128 + (uint8_t)(normalized * 127.0f);
    }
}

static int switch_pro_init(Controller *ctrl)
{
    SwitchProPrivate *priv = calloc(1, sizeof(SwitchProPrivate));
    if (!priv) return -1;

    /* Default calibration — will be overwritten if controller sends cal data */
    priv->cal_left.center_x = SWITCH_STICK_CENTER;
    priv->cal_left.center_y = SWITCH_STICK_CENTER;
    priv->cal_left.min_x    = SWITCH_STICK_MIN;
    priv->cal_left.min_y    = SWITCH_STICK_MIN;
    priv->cal_left.max_x    = SWITCH_STICK_MAX;
    priv->cal_left.max_y    = SWITCH_STICK_MAX;

    priv->cal_right = priv->cal_left;

    priv->output_counter = 0;
    priv->imu_enabled    = false;
    priv->vibration_enabled = false;

    ctrl->priv = priv;
    return 0;
}

/* Parse the standard 0x30 full input report */
static int parse_full_report(Controller *ctrl, const SwitchFullReport *rpt,
                             NormalizedInput *out)
{
    SwitchProPrivate *priv = ctrl->priv;
    uint16_t lx, ly, rx, ry;

    memset(out, 0, sizeof(*out));

    /* --- Buttons --- */

    /* Right-side buttons (byte 0) */
    out->cross    = !!(rpt->buttons[0] & SWITCH_BTN_B);  /* B → Cross */
    out->circle   = !!(rpt->buttons[0] & SWITCH_BTN_A);  /* A → Circle */
    out->square   = !!(rpt->buttons[0] & SWITCH_BTN_Y);  /* Y → Square */
    out->triangle = !!(rpt->buttons[0] & SWITCH_BTN_X);  /* X → Triangle */
    out->r1       = !!(rpt->buttons[0] & SWITCH_BTN_R);
    out->r2_analog = (rpt->buttons[0] & SWITCH_BTN_ZR) ? 255 : 0;

    /* Shared buttons (byte 1) */
    out->create    = !!(rpt->buttons[1] & SWITCH_BTN_MINUS);   /* - → Create */
    out->options   = !!(rpt->buttons[1] & SWITCH_BTN_PLUS);    /* + → Options */
    out->r3        = !!(rpt->buttons[1] & SWITCH_BTN_RSTICK);
    out->l3        = !!(rpt->buttons[1] & SWITCH_BTN_LSTICK);
    out->ps_button = !!(rpt->buttons[1] & SWITCH_BTN_HOME);
    out->touchpad  = !!(rpt->buttons[1] & SWITCH_BTN_CAPTURE); /* Capture → Touchpad */

    /* Left-side buttons (byte 2) */
    out->dpad_down  = !!(rpt->buttons[2] & SWITCH_BTN_DPAD_DOWN);
    out->dpad_up    = !!(rpt->buttons[2] & SWITCH_BTN_DPAD_UP);
    out->dpad_right = !!(rpt->buttons[2] & SWITCH_BTN_DPAD_RIGHT);
    out->dpad_left  = !!(rpt->buttons[2] & SWITCH_BTN_DPAD_LEFT);
    out->l1         = !!(rpt->buttons[2] & SWITCH_BTN_L);
    out->l2_analog  = (rpt->buttons[2] & SWITCH_BTN_ZL) ? 255 : 0;

    /* --- Sticks --- */
    decode_stick(rpt->left_stick, &lx, &ly);
    decode_stick(rpt->right_stick, &rx, &ry);

    out->left_stick_x  = scale_stick_axis(lx,
        priv->cal_left.center_x, priv->cal_left.min_x, priv->cal_left.max_x);
    out->left_stick_y  = 255 - scale_stick_axis(ly,
        priv->cal_left.center_y, priv->cal_left.min_y, priv->cal_left.max_y);
    out->right_stick_x = scale_stick_axis(rx,
        priv->cal_right.center_x, priv->cal_right.min_x, priv->cal_right.max_x);
    out->right_stick_y = 255 - scale_stick_axis(ry,
        priv->cal_right.center_y, priv->cal_right.min_y, priv->cal_right.max_y);

    /* --- IMU (use first sample) --- */
    if (priv->imu_enabled) {
        out->has_motion = true;

        /* Convert raw IMU to physical units */
        out->accel_x = (float)rpt->imu[0][0] / SWITCH_ACCEL_SCALE * 9.81f;
        out->accel_y = (float)rpt->imu[0][1] / SWITCH_ACCEL_SCALE * 9.81f;
        out->accel_z = (float)rpt->imu[0][2] / SWITCH_ACCEL_SCALE * 9.81f;

        out->gyro_x  = (float)rpt->imu[0][3] / SWITCH_GYRO_SCALE * (M_PI / 180.0f);
        out->gyro_y  = (float)rpt->imu[0][4] / SWITCH_GYRO_SCALE * (M_PI / 180.0f);
        out->gyro_z  = (float)rpt->imu[0][5] / SWITCH_GYRO_SCALE * (M_PI / 180.0f);
    }

    /* --- Battery --- */
    out->battery_level = ((rpt->battery_conn >> 4) & 0x0E) * 12;
    if (out->battery_level > 100) out->battery_level = 100;
    out->charging = !!(rpt->battery_conn & 0x10);

    return 0;
}

/* Parse the simple 0x3F USB report (before full mode is enabled) */
static int parse_simple_report(Controller *ctrl, const uint8_t *report, int len,
                               NormalizedInput *out)
{
    (void)ctrl;
    uint16_t buttons;
    uint8_t hat;

    if (len < 12) return -1;

    memset(out, 0, sizeof(*out));

    /* Simple report: [id=0x3F][buttons_lo][buttons_hi][hat][lx][ly][rx][ry]... */
    buttons = report[1] | (report[2] << 8);

    out->cross    = !!(buttons & 0x0004);  /* B */
    out->circle   = !!(buttons & 0x0008);  /* A */
    out->square   = !!(buttons & 0x0001);  /* Y */
    out->triangle = !!(buttons & 0x0002);  /* X */
    out->l1       = !!(buttons & 0x0010);
    out->r1       = !!(buttons & 0x0020);
    out->l2_analog = (buttons & 0x0040) ? 255 : 0;
    out->r2_analog = (buttons & 0x0080) ? 255 : 0;
    out->create   = !!(buttons & 0x0100);  /* - */
    out->options  = !!(buttons & 0x0200);  /* + */
    out->l3       = !!(buttons & 0x0400);
    out->r3       = !!(buttons & 0x0800);
    out->ps_button = !!(buttons & 0x1000); /* Home */
    out->touchpad  = !!(buttons & 0x2000); /* Capture */

    /* Hat switch for D-pad */
    hat = report[3];
    out->dpad_up    = (hat == 0 || hat == 1 || hat == 7);
    out->dpad_right = (hat == 1 || hat == 2 || hat == 3);
    out->dpad_down  = (hat == 3 || hat == 4 || hat == 5);
    out->dpad_left  = (hat == 5 || hat == 6 || hat == 7);

    /* Sticks: simple mode uses 0..255 natively */
    out->left_stick_x  = report[4];
    out->left_stick_y  = report[5];
    out->right_stick_x = report[6];
    out->right_stick_y = report[7];

    return 0;
}

static int switch_pro_parse_input(Controller *ctrl,
                                  const uint8_t *report, int len,
                                  NormalizedInput *out)
{
    if (len < 1) return -1;

    switch (report[0]) {
    case SWITCH_REPORT_INPUT_FULL:
        if (len < (int)sizeof(SwitchFullReport)) return -1;
        return parse_full_report(ctrl, (const SwitchFullReport *)report, out);

    case SWITCH_REPORT_INPUT_SIMPLE:
        return parse_simple_report(ctrl, report, len, out);

    case SWITCH_REPORT_SUBCMD_REPLY:
        /* Subcommand replies also contain input data at the same offsets
         * as full reports, but we primarily use them for initialization.
         * Parse input from them when they arrive. */
        if (len >= (int)sizeof(SwitchFullReport))
            return parse_full_report(ctrl, (const SwitchFullReport *)report, out);
        return -1;

    default:
        return -1;
    }
}

/* Build subcommand output report */
static int build_subcmd(SwitchProPrivate *priv, uint8_t subcmd,
                        const uint8_t *subcmd_data, int data_len,
                        uint8_t *buf, int max_len)
{
    int total = 10 + 1 + data_len;
    if (max_len < total) return 0;

    memset(buf, 0, total);
    buf[0] = SWITCH_REPORT_OUTPUT;
    buf[1] = priv->output_counter++ & 0x0F;
    /* bytes 2..9: rumble data (neutral) */
    buf[2] = 0x00; buf[3] = 0x01; buf[4] = 0x40; buf[5] = 0x40;
    buf[6] = 0x00; buf[7] = 0x01; buf[8] = 0x40; buf[9] = 0x40;
    buf[10] = subcmd;
    if (subcmd_data && data_len > 0)
        memcpy(&buf[11], subcmd_data, data_len);

    return total;
}

static int switch_pro_build_output(Controller *ctrl,
                                   uint8_t large_motor, uint8_t small_motor,
                                   uint8_t r, uint8_t g, uint8_t b,
                                   uint8_t *buf, int max_len)
{
    SwitchProPrivate *priv = ctrl->priv;
    (void)r; (void)g; (void)b;

    if (max_len < 10) return 0;

    /* Switch Pro rumble encoding (simplified HD rumble) */
    memset(buf, 0, 10);
    buf[0] = SWITCH_REPORT_RUMBLE_ONLY;
    buf[1] = priv->output_counter++ & 0x0F;

    if (large_motor > 0 || small_motor > 0) {
        /* Low-band frequency ~160Hz for large motor, high-band ~320Hz for small */
        float low_amp  = (float)large_motor / 255.0f;
        float high_amp = (float)small_motor / 255.0f;

        /* Encode amplitude (simplified — proper HD rumble has complex encoding) */
        uint8_t low_byte  = (uint8_t)(low_amp * 0xC0);
        uint8_t high_byte = (uint8_t)(high_amp * 0x60);

        /* Left actuator */
        buf[2] = 0x28;     /* Low freq */
        buf[3] = 0x88 | (low_byte >> 6);
        buf[4] = high_byte;
        buf[5] = 0x60;     /* High freq */

        /* Right actuator (same) */
        buf[6] = buf[2];
        buf[7] = buf[3];
        buf[8] = buf[4];
        buf[9] = buf[5];
    } else {
        buf[2] = 0x00; buf[3] = 0x01; buf[4] = 0x40; buf[5] = 0x40;
        buf[6] = 0x00; buf[7] = 0x01; buf[8] = 0x40; buf[9] = 0x40;
    }

    return 10;
}

static void switch_pro_cleanup(Controller *ctrl)
{
    free(ctrl->priv);
    ctrl->priv = NULL;
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

/* Read reports until one has report[0] == b0 and report[idx] == val. */
static bool wait_reply(int fd, uint8_t b0, int idx, uint8_t val, int timeout_ms)
{
    uint64_t end = now_ms() + timeout_ms;
    uint8_t in[64];
    while (now_ms() < end) {
        int n = usb_read_report(fd, in, sizeof(in));
        if (n < 0) return false;
        if (n > idx && in[0] == b0 && in[idx] == val) return true;
        if (n == 0) usleep(1000);
    }
    return false;
}

/* USB-only commands: 0x80 0x02 handshake, 0x80 0x04 disables the
 * controller's USB timeout. Without them it re-enumerates every second. */
static bool usb_command(Controller *ctrl, uint8_t cmd, bool expect_ack)
{
    uint8_t out[2] = {0x80, cmd};
    if (usb_write_report(ctrl->fd, out, sizeof(out)) != (int)sizeof(out))
        return false;
    return !expect_ack || wait_reply(ctrl->fd, 0x81, 1, cmd, 300);
}

static bool subcommand(Controller *ctrl, uint8_t id, uint8_t arg)
{
    uint8_t buf[64];
    int len = build_subcmd(ctrl->priv, id, &arg, 1, buf, sizeof(buf));
    if (usb_write_report(ctrl->fd, buf, len) != len)
        return false;
    /* 0x21 reply carries the acknowledged subcommand id at byte 14 */
    return wait_reply(ctrl->fd, SWITCH_REPORT_SUBCMD_REPLY, 14, id, 300);
}

int switch_pro_setup(Controller *ctrl)
{
    SwitchProPrivate *priv = ctrl->priv;

    /* Same sequence as Linux hid-nintendo: status, handshake, 3Mbit,
     * handshake, then disable the USB timeout. */
    usb_command(ctrl, 0x01, true);
    if (!usb_command(ctrl, 0x02, true)) {
        klog_printf("[AnyController] switch: USB handshake not acknowledged\n");
        return -1;
    }
    bool fast = usb_command(ctrl, 0x03, true);
    bool hs2  = usb_command(ctrl, 0x02, true);
    bool notimeout = usb_command(ctrl, 0x04, false);
    usleep(50000);
    klog_printf("[AnyController] switch: usb 3mbit=%d handshake2=%d no_timeout_sent=%d\n",
                fast, hs2, notimeout);

    for (int attempt = 0; attempt < 3 && !priv->imu_enabled; attempt++)
        priv->imu_enabled = subcommand(ctrl, SWITCH_SUBCMD_ENABLE_IMU, 0x01);
    priv->vibration_enabled = subcommand(ctrl, SWITCH_SUBCMD_ENABLE_VIBRATION, 0x01);
    bool full = subcommand(ctrl, SWITCH_SUBCMD_SET_INPUT_MODE, SWITCH_REPORT_INPUT_FULL);
    bool leds = subcommand(ctrl, SWITCH_SUBCMD_SET_PLAYER_LEDS, (1 << ctrl->slot) & 0x0F);

    klog_printf("[AnyController] switch: setup imu=%d vib=%d full=%d leds=%d\n",
                priv->imu_enabled, priv->vibration_enabled, full, leds);
    return 0;
}

/* Static driver instance */
static const ControllerDriver switch_pro_driver = {
    .name       = "Switch Pro Controller",
    .type       = CTRL_TYPE_SWITCH_PRO,
    .vendor_id  = SWITCH_VID,
    .product_id = SWITCH_PRO_PID,
    .init       = switch_pro_init,
    .setup      = switch_pro_setup,
    .parse_input = switch_pro_parse_input,
    .build_output = switch_pro_build_output,
    .cleanup    = switch_pro_cleanup,
};

const ControllerDriver *switch_pro_get_driver(void)
{
    return &switch_pro_driver;
}
