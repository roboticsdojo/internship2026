# Robot 2 Arduino Firmware

Arduino Uno firmware for Robot 2's low-level motor/encoder control.
Communicates with the Pi over USB serial; the ROS2 side is the
`robot_2_hardware` package (`src/robot_2_hardware/`), which sends
velocity targets and polls encoder ticks using the protocol below.

**Current version: `Robot2Firmware_v2/`**

## Hardware

- Arduino Uno
- 2x L298N motor drivers (one per side; each drives both the front
  and rear motor on that side in parallel)
- 2x quadrature wheel encoders (rear left, rear right)
- Motor supply: L298N logic (`5V` pin) and encoder VCC are powered
  from a dedicated buck converter stepping the main battery down to
  5V - **not** from the Arduino's onboard 5V regulator and **not**
  from the L298N's own onboard regulator (5V-EN jumper removed on
  both boards). This was a fix for motor-driver logic glitching
  under load; see pin/wiring notes in `config.h`.

Pin assignments, encoder wiring, and the full pinout are documented
in `Robot2Firmware_v2/config.h` and `Robot2Firmware_v2/motor_driver.h`.

## Uploading

1. Open `Robot2Firmware_v2/Robot2Firmware_v2.ino` in the Arduino IDE
   (all `.h`/`.cpp` files in the same folder are picked up
   automatically as sketch tabs).
2. Select board: Arduino Uno.
3. Select the correct port (`/dev/ttyACM0` on Linux, unless remapped
   by a udev rule - see the `robot_2_hardware` README for the
   `/dev/ttyUSB_ARDUINO` rule used on the Pi).
4. Upload.
5. Serial Monitor, if testing manually: **57600 baud**, line ending
   set to **Newline** (or **Both NL & CR**) - commands are only
   processed on `\n`/`\r`, so "No line ending" will silently do
   nothing.

## Serial protocol

Single-character command + optional space-separated float args,
newline-terminated. All commands and responses are plain ASCII, no
"OK"/ready banners on startup (kept out intentionally so the serial
buffer stays clean for ROS2).

| Command | Args | Effect | Response |
|---|---|---|---|
| `m <left> <right>` | ticks/PID-period target for each side | Sets PID velocity targets. Switches to closed-loop PID mode. Resets the watchdog. | none |
| `o <left> <right>` | raw PWM (-255 to 255) per side | Bypasses PID, drives motors directly at this PWM. Switches to open-loop mode so the 20Hz PID loop won't immediately overwrite it. Resets the watchdog. | none |
| `e` | none | Requests cumulative encoder ticks. Does **not** reset the watchdog (so polling encoders alone won't keep a dead motion-command link alive). | `<left_ticks> <right_ticks>\n` |
| `r` | none | Resets both encoder counters to zero. | none |
| `s` | none | Stops both motors immediately, resets PID state. | none |
| `p <kp> <ki> <kd>` | PID gains | Sets tunings for both wheels' PID controllers. | none |

### Safety watchdog

If no `m` or `o` command arrives for 2 seconds, motors are forced to
stop and PID state is reset. Encoder polling (`e`) intentionally does
NOT reset this timer - if only `e` requests kept arriving (e.g. from
an odometry loop) while the actual motion-command link had died, the
robot would otherwise never realize the link was dead.
