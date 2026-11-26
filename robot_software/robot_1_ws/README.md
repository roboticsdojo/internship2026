# Auracle
This repository contains code for Auracle, an autonomous waste-sorting robot built for the Robotics Dojo Competition 2026 (theme: Autonomous Waste Management). A camera reads a waste class (metal, organic or plastic) off a screen, and the robot navigates to the matching loading region, collects the item, and delivers it to the matching offload region.

**Features**
- ROS 2 Jazzy, Raspberry Pi on the robot, laptop/PC (WSL or Linux) for RViz and teleop
- Arduino Nano firmware for motors, encoders and IMU
- SLAM Toolbox + Nav2 + AMCL for mapping and navigation
- YOLO (ONNX) waste detector and a BehaviorTree.CPP mission runner

## Team
| Name | Course of study | Email |
|---|---|---|
| Fredrick Wanjeri | B.Sc Mechatronic Engineering | wanjerifredrick@gmail.com |
| Emmanuel Mututa | B.Sc Mechatronic Engineering | emmanuaelmututa25@gmail.com |
| Amy Kibara | B.Sc Mechatronic Engineering | amy.kibara@gmail.com |
| Jacque Kyalo | B.Sc Electrical & Electronic Engineering | jacquekyalo367@gmail.com |


## Hardware
| Part | Notes |
|---|---|
| Compute | Raspberry Pi 5 (4GB RAM) on the robot, PC/laptop for RViz|
| Microcontroller | Arduino Nano v3 |
| Drive | 4 x 12 V geared DC motors, 2 x dual H-bridge drivers, encoders on the rear wheels |
| Sensors | RPLidar A1, MPU6050 IMU, Pi camera |
| Actuators | servo for the loading mechanism |

## Pinout
| Function | Pin |
|---|---|
| Right front / right back PWM | D5 / D6 |
| Right direction (fwd / rev) | D7 / D8 |
| Left back / left front PWM | D9 / D10 |
| Left direction (fwd / rev) | A0 / A1 |
| Rear-right encoder A / B | D2 / D4 |
| Rear-left encoder A / B | D12 / D3 |
| Servo signal | D13 |
| IMU SDA / SCL | A4 / D11 (software I2C) |

The IMU uses bit-banged I2C because the Nano's A5 (hardware SCL) is damaged, so `Wire` is not used. Motor, encoder and IMU pins and all tuning constants live in `firmware/auracle_firmware/auracle_firmware/config.h`.

## Repository layout
```
firmware/auracle_firmware/   Arduino Nano firmware (PlatformIO / arduino-cli)
network_files/pc/            PC scripts: connect_pi.sh (Zenoh/ROS env), sync_time.sh (clock sync), debug_time.sh (sync check)
network_files/pi/            zenoh-router.service (starts the Zenoh router on the Pi)
src/
  auracle_description/       URDF/xacro, Gazebo world and gamefield model
  auracle_hardware/          ros2_control hardware plugin (talks to the Nano over serial)
  auracle_bringup/           Launch files, controller/EKF/SLAM/Nav2 configs, teleop scripts, udev rules
  auracle_bt/                BehaviorTree.CPP mission runner, trees and region config
  auracle_vision/            Camera node, ONNX waste detector, models and eval tools
  rf2o_laser_odometry/       Vendored third-party lidar odometry (see its LICENSE)
```

## Networking and time
ROS 2 uses **rmw_zenoh** with a Zenoh router running on the Pi as a systemd service. All machines must share the same `ROS_DOMAIN_ID` (89 in this repo). Clocks are synced with **chrony**, with the PC as the time server, which keeps TF and sensor fusion consistent without internet.
 
Three scripts in `network_files/pc/` handle this. `connect_pi.sh` finds the Pi, sets `RMW_IMPLEMENTATION`, `ROS_DOMAIN_ID` and `ZENOH_CONFIG_OVERRIDE`. `sync_time.sh` starts chrony on the PC and snaps both clocks. `debug_time.sh` reports whether the sync is healthy. All three find the Pi at `auracle.local`, or take its IP as the first argument. `connect_pi.sh` and `sync_time.sh` must be **sourced**, not executed, so the variables stay in your shell, while `debug_time.sh` is run normally.

## Firmware
The Nano runs a 40 Hz control loop (25 ms period): per-wheel PI control with feedforward, soft start and kick, stall/fault detection, encoder interrupts and a raw-register MPU6050 driver over software I2C. It talks to the Pi over a plain ASCII serial protocol at 500000 baud (`v L R` commands, `e ...` telemetry, `F ...` faults). On the Pi side, `auracle_hardware` exposes it to `ros2_control` as a diff-drive base plus an IMU. Commands double as a 500 ms watchdog heartbeat.

## Teleop, SLAM, Navigation
- **Command routing:** `twist_mux` gives joystick/keyboard priority over Nav2. `diff_drive_controller` runs from `my_controllers.yaml` with calibrated real-robot overrides in `my_controllers_real.yaml`.
- **Odometry:** EKF fuses wheel odometry, rf2o lidar odometry and the IMU.
- **Teleop:** keyboard (`key_drive.py` or `teleop_twist_keyboard`) or a gamepad via a UDP bridge (`joy_udp_node.py`, `joy_drive.py`).
- **Mapping:** `slam_toolbox` online async, fed by a box-filtered scan (`scan_filtered`) that removes the robot's own body.
- **Navigation:** Nav2 with NavFn, Regulated Pure Pursuit and AMCL on a saved map.

## Computer Vision
`auracle_vision` runs a YOLO model exported to ONNX (default `waste_yolo26n_320.onnx`, 320 px input) at 5 Hz, only while the behavior tree asks for a detection. A rolling vote (6 of the last 10 frames, mean score >= 0.35) produces one decision on `/auracle/vision/decision`. Tools in `tools/` resize/quantize models and simulate screen-photographed input.

## Behaviour Trees
`auracle_bt` sits above Nav2 and never drives directly. `bt_runner` loads an XML tree and calls `NavigateThroughPoses`.
 
| Tree | Purpose |
|---|---|
| `region_tree.xml` | Default. Wait for a region number, go to LOAD N, then OFFLOAD N, then home |
| `vision_tree.xml` | Same loop, but the camera picks the region (`metal=1 organic=2 plastic=3`, editable in the XML) |
| `mark_tree.xml` | Record the six regions by clicking them in RViz |
| `region_click_tree.xml`, `vision_click_tree.xml` | Same as above with a manual 2D Pose Estimate |
| `goal_tree.xml` | Drive to each 2D Goal Pose you click |
 
Regions are stored in `config/regions.yaml`.

## Setup
**Both machines**
 
```bash
sudo apt install ros-jazzy-rmw-zenoh-cpp ros-jazzy-navigation2 ros-jazzy-nav2-bringup \
  ros-jazzy-slam-toolbox ros-jazzy-laser-filters ros-jazzy-robot-localization \
  ros-jazzy-teleop-twist-keyboard ros-jazzy-twist-stamper chrony
```
 
**Pi only**
 
```bash
sudo apt install ros-jazzy-rplidar-ros ros-jazzy-camera-ros ros-jazzy-vision-msgs ros-jazzy-cv-bridge
pip install onnxruntime --break-system-packages
sudo cp src/auracle_bringup/config/99-auracle.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger   # creates /dev/arduino and /dev/rplidar
```

**Connection steps**
1. Install the Zenoh router service on the Pi (edit `User=` in the file if your username isn't `auracle`):
   ```bash
   sudo cp network_files/pi/zenoh-router.service /etc/systemd/system/
   sudo systemctl daemon-reload && sudo systemctl enable --now zenoh-router.service
   systemctl status zenoh-router.service   # should be active
   ```
2. Add `export RMW_IMPLEMENTATION=rmw_zenoh_cpp` and `export ROS_DOMAIN_ID=89` to the Pi's `~/.bashrc`.
3. On the Pi, point chrony at the PC only (disable the default NTP pools and add `sourcedir /etc/chrony/sources.d`). `sync_time.sh` writes the PC address into that folder.
4. On the PC, add `allow all` and `local stratum 10` to `/etc/chrony/chrony.conf`. If you use WSL2, enable `networkingMode=mirrored` in `.wslconfig` and open UDP 123 in the Windows firewall.
5. Run `ssh-copy-id auracle@auracle.local` once from the PC.
6. Turn off Pi Wi-Fi power save, which otherwise adds latency spikes: `sudo iw dev wlan0 set power_save off`. To keep it off across reboots, put `[connection]` and `wifi.powersave = 2` in `/etc/NetworkManager/conf.d/wifi-powersave-off.conf`.

**Build** (on both machines, from the workspace root)
```bash
colcon build --symlink-install
```
 
**Flash the firmware** (stop ROS first, only one program can own the serial port)
 
```bash
arduino-cli compile -b arduino:avr:nano:cpu=atmega328 --output-dir build firmware/auracle_firmware/auracle_firmware
arduino-cli upload  -b arduino:avr:nano:cpu=atmega328 -p /dev/arduino --input-dir build
```

If the upload fails with a sync error, your Nano has the old bootloader: use `cpu=atmega328old` in both commands.

## Running the robot
 
Every PC terminal needs the environment first, and the clocks must be synced once per session:
 
```bash
source install/setup.bash && source network_files/pc/connect_pi.sh
source network_files/pc/sync_time.sh      # once per session
network_files/pc/debug_time.sh            # optional: confirm the sync
```
 
If `auracle.local` does not resolve (common from WSL), pass the Pi's IP, for example `source network_files/pc/connect_pi.sh 192.168.1.50`.
 
**1. Start the robot (Pi)**
 
```bash
chronyc waitsync 9 0.1 && source install/setup.bash && \
ros2 launch auracle_bringup launch_robot.launch.py namespace:=auracle use_joy_node:=false
```
 
Check that `diff_cont`, `joint_broad` and `imu_broadcaster` are active:
 
```bash
ros2 control list_controllers -c /auracle/controller_manager
```
 
**2. Teleop**
 
```bash
ros2 run auracle_bringup key_drive.py --ros-args -r cmd_vel:=cmd_vel_joy -r __ns:=/auracle
```
 
**3. Map the arena** (SLAM on the Pi, RViz on the PC)
 
```bash
# Pi
ros2 launch auracle_bringup slam_mapping.launch.py namespace:=auracle use_rviz:=false
# PC
ros2 launch auracle_bringup slam_mapping.launch.py namespace:=auracle use_slam:=false
# Pi, when done
ros2 run nav2_map_server map_saver_cli -f ~/maps/arena --ros-args -r map:=/auracle/map
```
 
**4. Navigate** (stop teleop and SLAM first, since teleop overrides Nav2)
 
```bash
# Pi
ros2 launch auracle_bringup localization_nav_rviz.launch.py namespace:=auracle use_rviz:=false map:=$HOME/maps/arena.yaml
# PC
ros2 launch auracle_bringup localization_nav_rviz.launch.py namespace:=auracle use_nav:=false
```
 
In RViz, set the robot's pose with **2D Pose Estimate**, then send a **2D Goal Pose**.
 
This launch file runs Nav2's collision monitor by default, which hard-stops the robot on the live lidar scan. Add `use_collision_monitor:=false` to the Pi command to turn it off.
 
**5. Run the mission**
 
Mark the regions once per map, with the robot parked at the start:
 
```bash
# Pi
ros2 launch auracle_bt mission.launch.py map:=$HOME/maps/arena.yaml route_file:=$HOME/maps/arena_regions.yaml mark:=true
# PC
ros2 launch auracle_bt mission_rviz.launch.py
```
 
With the **Publish Point** tool, click the centres of LOAD 1, 2, 3 and then OFFLOAD 1, 2, 3. The runner saves the file and exits.
 
Set the start pose once per map. Park the robot where mapping began and launch with a manual pose click:
 
```bash
# Pi
ros2 launch auracle_bt mission.launch.py map:=$HOME/maps/arena.yaml route_file:=$HOME/maps/arena_regions.yaml auto_pose:=false use_collision_monitor:=true
# PC
ros2 launch auracle_bt mission_rviz.launch.py
```
 
In RViz, click **2D Pose Estimate** at the robot's real position and drag along its heading. Then read where AMCL puts it:
 
```bash
ros2 topic echo /auracle/amcl_pose --once
```
 
Note `position.x`, `position.y` and `yaw = 2 * atan2(orientation.z, orientation.w)`, and write them into `start` and `home` in `arena_regions.yaml`:
 
```yaml
start: {x: <x>, y: <y>, yaw: <yaw>}
home:
  - {x: <x>, y: <y>, yaw: <yaw>}
```
 
Run the mission, choosing the first region with `route:=N` (later regions are sent on the `/auracle/region` topic):
 
```bash
ros2 launch auracle_bt mission.launch.py map:=$HOME/maps/arena.yaml route_file:=$HOME/maps/arena_regions.yaml route:=1 use_collision_monitor:=true
ros2 topic pub --once /auracle/region std_msgs/msg/Int32 "{data: 2}"   # next region, from another terminal
```
 
Let the camera choose the region instead:
 
```bash
ros2 launch auracle_bt mission.launch.py map:=$HOME/maps/arena.yaml route_file:=$HOME/maps/arena_regions.yaml vision:=true use_collision_monitor:=true
```
 
`use_collision_monitor` is **off by default in `mission.launch.py`**. The commands above turn it on (Nav2 then publishes to `cmd_vel_nav_raw` and the monitor forwards to `cmd_vel_nav`). Leave the flag out to let Nav2 drive `twist_mux` directly.
 
Bench-test the detector alone:
 
```bash
ros2 launch auracle_vision vision.launch.py always_on:=true
ros2 topic echo /auracle/vision/decision
```
 
**Simulation (Gazebo)**
 
```bash
ros2 launch auracle_bringup launch_sim.launch.py
```
The same commands for teleop, SLAM and navigation apply for the simulation.

## Known limitations
 
- The rear-right motor is weak going forward, which caps the whole robot at about 0.16 m/s until it is replaced.
- The waste classifier is only about 50 % accurate on clean test photos and about 31-35 % per frame on simulated screen photos. Fine-tune it on frames captured from the real screen (`save_dir:=...` on the detector) before relying on it.
- Left turns are the weakest manoeuvre (weak left-front motor).

## Credits and licenses
 
The Auracle packages (`auracle_bringup`, `auracle_bt`, `auracle_description`, `auracle_hardware`, `auracle_vision`) and the firmware are released under the Apache License 2.0. `rf2o_laser_odometry` is third-party code by its original authors and remains under its own license (GPLv3, see `src/rf2o_laser_odometry/LICENSE`).