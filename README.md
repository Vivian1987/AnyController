# AnyController

Use non-PlayStation controllers on a jailbroken PS5. AnyController is an ELF payload that reads a USB controller and registers it with the PS5 as a **virtual DualSense**, so the whole system sees a normal DualSense. That includes the home screen, the user picker, the PS button menu and every game, with no game patching.

## Supported controllers

| Controller | Connection | Status |
|---|---|---|
| Nintendo Switch Pro Controller (`057e:2009`) | USB cable | Working |
| Xbox 360 / XInput controllers (`045e:028e`) | USB cable or 2.4GHz dongle | Working |
| GameSir T4 Pro | 2.4GHz dongle (X-input mode) | Working |

Many third-party pads and dongles have an X-input mode that reports as an Xbox 360 controller, so they should work too.

Up to 4 controllers can be connected at once. Each one becomes its own DualSense, so local co-op works alongside a real DualSense.

Tested on PS5 firmware **13.60**.

## Requirements

- A jailbroken PS5 running an ELF loader that accepts payloads on port 9021, such as [elfldr](https://github.com/ps5-payload-dev/elfldr).
- The controller plugged into one of the PS5's USB ports (or its dongle, for wireless pads).

## Usage

1. Build `anycontroller.elf` (see [Building](#building)).
2. Send it to the console:
   ```bash
   python3 send_payload.py <PS5_IP> anycontroller.elf
   ```
3. A notification says AnyController is running. Plug in the controller. You'll get a "connected as DualSense" notification and the user picker, the same as with a real DualSense.

Sending the payload again replaces the copy that's already running.

## Button mapping

Buttons are mapped **by position**, so the bottom face button is always Cross:

| PS5 | Switch Pro | Xbox / XInput |
|---|---|---|
| Cross ✕ | B | A |
| Circle ○ | A | B |
| Square □ | Y | X |
| Triangle △ | X | Y |
| L1 / R1 | L / R | LB / RB |
| L2 / R2 | ZL / ZR (digital) | LT / RT (analog) |
| L3 / R3 | Stick clicks | Stick clicks |
| Options | + | Start |
| Create | − | Back |
| Touchpad click | Capture | — |
| PS button | Home | Guide |

## Building

Requires the PS5 payload SDK from **[ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk)**, v0.45 or newer. Its README covers installation (clang-18 and lld-18 on Linux or WSL).

> The older `john-tornblom/ps5-payload-sdk` repository stopped being updated in 2024. Payloads built with it crash in their startup code under current ELF loaders.

```bash
export PS5_PAYLOAD_SDK=/path/to/installed/sdk
make
make send PS5_HOST=<PS5_IP>
```

On Windows, `build_ps5.sh` runs the same build from WSL.

## How it works

- **Virtual DualSense:** `/dev/hid` accepts three ioctls (add device, insert input, delete device) that libScePad uses for virtual pads, and it doesn't check who is calling. A payload can register a pad the kernel treats as a real DualSense. Interface discovered by [FGG-XSense](https://github.com/FGGstore/FGG-XSense). See [`src/vpad.c`](src/vpad.c).
- **USB access:** the PS5 has no `uhid` device. Controllers appear only as FreeBSD `ugen` nodes (`/dev/usb/B.A.E`). AnyController identifies devices by their USB descriptor, detaches Sony's HID driver, claims the interface and reads reports from the interrupt endpoints. See [`src/transport/usb.c`](src/transport/usb.c).
- **Switch Pro Controller:** over USB it drops off the bus every second or so until the host sends the USB handshake and the "no timeout" command. The driver sends the same sequence as Linux's `hid-nintendo`, then enables full 0x30 reports and the motion sensor. See [`src/controllers/switch_pro.c`](src/controllers/switch_pro.c).
- **Xbox 360 / XInput:** standard 20-byte input reports. The driver sends the player-LED command on connect, which also makes multi-mode dongles stay in X-input mode instead of cycling. See [`src/controllers/xbox360.c`](src/controllers/xbox360.c).
- **Brief drops:** if a controller drops off USB, its virtual DualSense is kept for 3 seconds. A quick reconnect then resumes on the same pad, without a disconnect or user re-pick.

### Adding a controller

Each controller type is a `ControllerDriver` (see [`include/controller.h`](include/controller.h)): a VID/PID, an optional `setup` step for handshakes, and a `parse_input` that turns raw reports into a `NormalizedInput`. Register it in [`src/controller.c`](src/controller.c).

## Debugging

AnyController logs to the kernel log. With a klog server running on the console (port 3232), you'll see attach and detach events, per-5-second report counts and any errors:

```
[AnyController] usb 2.2: opened Switch Pro Controller (057e:2009)
[AnyController] slot 0: Switch Pro Controller -> vpad handle=14877453
[AnyController] slot 0: 751 reports/5s, 0 submit failures
```

## Not yet supported

- Bluetooth controllers
- Rumble from games, and the light bar
- Motion controls (gyro)
- Other controller families (Xbox One/Series, DualShock 4, generic HID gamepads)
- The Create button bit is unverified

## Credits

- [FGG-XSense](https://github.com/FGGstore/FGG-XSense): discovery of the `/dev/hid` virtual pad interface
- [ps5-payload-dev](https://github.com/ps5-payload-dev): the payload SDK, ELF loader and klog server
- [dekuNukem/Nintendo_Switch_Reverse_Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering): Switch controller protocol
- Linux `hid-nintendo` and `xpad` drivers: reference handshake sequences
- [MissionControl](https://github.com/ndeadly/MissionControl) and [PoorDS4](https://github.com/ItsBlurf/PoorDS4): inspiration

## License

GPL-3.0. See [LICENSE](LICENSE).

Not affiliated with Sony, Nintendo or Microsoft.
