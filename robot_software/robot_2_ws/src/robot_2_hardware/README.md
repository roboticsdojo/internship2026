# robot_2_hardware

`ros2_control` `SystemInterface` plugin that bridges ros2_control to
Robot 2's real hardware: an Arduino Uno running the firmware in
`robot_2_ws/firmware/Robot2Firmware_v2/`, two L298N motor drivers, and
two quadrature wheel encoders. This package is only used when
`ros2_control.xacro` is built with `sim_mode:=false`; simulation uses
`gz_ros2_control/GazeboSimSystem` instead and does not depend on this
package at all.

## Responsibility

- Opens and configures the serial connection to the Arduino
  (`on_configure`/`on_activate`/`on_deactivate`).
- On every controller-manager cycle:
  - `read()`: requests cumulative encoder ticks from the Arduino
    (`e` command), converts them to wheel position (rad) and velocity
    (rad/s) for the 4 wheel joints.
  - `write()`: averages the front/rear command for each side, applies
    direction-correction signs, converts rad/s to Arduino PID
    ticks-per-period, and sends a velocity target (`m` command).
- The Arduino's own onboard PID loop (in firmware) handles the
  fast, per-wheel velocity regulation between ros2_control cycles;
  this plugin only sends targets and reads back ticks — see
  `robot_2_ws/firmware/Robot2Firmware_v2/serial_protocol.cpp` for the
  full command set (`e`, `m`, `o`, `p`, `r`, `s`).

## Topics

This plugin does not publish or subscribe to any topics directly —
it is a `ros2_control` hardware component, not a node. The topics
that exist when it's active come from the controllers loaded on top
of it (see `robot_2_bringup/launch/launch_robot.launch.py` and
`robot_2_bringup/config/my_controllers_real.yaml`):

| Topic | Published by | Notes |
|---|---|---|
| `/robot_2/cmd_vel` | (subscribed) `diff_cont` | Remapped from `diff_cont`'s real topic `/robot_2/cmd_vel`, same name used in sim, so teleop/nav targets the same topic in both modes. `TwistStamped` only (Jazzy `diff_drive_controller` requirement). |
| `/robot_2/odom` | `diff_cont` | Remapped from `/robot_2/diff_cont/odom`. |
| `/robot_2/joint_states` | `joint_broad` | Remapped from `/joint_states`. |

## Hardware parameters

Declared per-instance in `robot_2_description/urdf/ros2_control.xacro`
(inside the `<xacro:unless value="$(arg sim_mode)">` block), not in a
controller `config/*.yaml` file — these are `ros2_control` **hardware**
parameters, read from the URDF via `HardwareInfo::hardware_parameters`
in `on_init()`, which is a separate mechanism from ROS2's normal
parameter server that controller yaml files configure. All have code
defaults (see `getNumericParameter`/`getStringParameter` calls in
`robot_2_system.cpp`) so the xacro only needs to set a value where it
differs from the default.

| Parameter | Default | Meaning |
|---|---|---|
| `device` | `/dev/ttyACM0` | Serial device path. This repo's xacro sets it to `/dev/ttyUSB_ARDUINO`, a udev-rule symlink matching the Arduino's USB vendor/product ID, so it's stable across reboots regardless of which `/dev/ttyACM*` the kernel assigns. |
| `baud_rate` | `57600` | Must match `SERIAL_BAUD` in the Arduino firmware's `config.h`. |
| `timeout_ms` | `200` | Max wait for the Arduino's encoder response before `read()` reports a failure. |
| `encoder_ticks_per_rev` | `1980.0` | Must match `ENCODER_TICKS_PER_REV` in firmware `config.h`. |
| `pid_period` | `0.05` (seconds) | Must match the Arduino's `PID_PERIOD` (ms) in `config.h`, converted to seconds. Used to convert rad/s targets to ticks-per-period. |
| `wheel_radius` | `0.0425` | Meters. Also set independently in `my_controllers.yaml`/`my_controllers_real.yaml` for `diff_cont`'s kinematics - the two must be kept in sync if the wheel changes. |
| `wheel_separation` | `0.30` | Meters. Currently unused by this plugin directly (kept for parity with `diff_cont`'s own parameter of the same name) but read at `on_init()` in case future revisions need it here. |
| `left_encoder_sign`, `right_encoder_sign` | `1.0` | Set to `-1.0` to flip an encoder's counting direction without touching firmware, if a side reads backwards. |
| `left_command_sign`, `right_command_sign` | `1.0` | Set to `-1.0` to flip a side's drive direction without touching firmware, if a side drives backwards. |

## Firmware constants (not ROS parameters)

`robot_2_ws/firmware/Robot2Firmware_v2/config.h` hardcodes several
values (`MOTOR_MAX_PWM`, `MOTOR_MIN_PWM`, `PID_PERIOD`,
`ENCODER_TICKS_PER_REV`, pin assignments). These are compiled into the
Arduino at flash time, not passed at runtime, since the Arduino has no
ROS2 parameter server - this is standard for embedded firmware
calibration constants. `MOTOR_MIN_PWM` in particular is a
measured-from-testing deadband value specific to this chassis's
motors; re-measure and update it if motors are swapped.

## Testing on real hardware

See the top-level `robot_2_ws/README.md` (or
`robot_2_bringup/launch/launch_robot.launch.py`'s header comment) for
the exact launch and teleop commands. In short:

```bash
ros2 launch robot_2_bringup launch_robot.launch.py
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p stamped:=true -r /cmd_vel:=/robot_2/cmd_vel
```

Requires the Arduino connected via USB with the `/dev/ttyUSB_ARDUINO`
udev rule in place (see udev rule matching Arduino Uno R3's
`idVendor=2341`, `idProduct=0043`).