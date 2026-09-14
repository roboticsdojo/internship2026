There are four stages:
1. SLAM (online_async_launch.py, via slam_toolbox) — you have no map yet. The robot builds one from laser scans + odometry while also tracking itself within the map-in-progress. Run this once (or whenever you need to re-map / extend the map).
2. Save the map — once SLAM has built a good map, you save it to disk as a .yaml + .pgm pair. This is a one-off step, not something that runs continuously.
3. Localization (localization_neo_launch.py, via neo_localization2) — you already have a saved map. This node's only job is figuring out where the robot currently is inside that fixed map, using the laser scan. Nothing is being built or changed.
4. Navigation (navigation_launch.py, via Nav2) — path planning and execution (controller_server, planner_server, bt_navigator, etc). It doesn't care whether the pose it's given came from SLAM or from localization — it just needs some accurate map → odom tf and consumes that the same way either time. This is also the layer autonomy (waypoint following, exploration, task queues, etc.) gets built on top of.

# Building - either
```
cd ~/Documents/auracle_ws
rm -rf build install log
colcon build --symlink-install && source install/setup.bash
```

# Simulation 
## Terminal 1 (Gazebo + robot)
This launches the robot in Gazebo with use_sim_time:=true baked in. Build and save a map
```
source install/setup.bash && ros2 launch auracle_bringup launch_sim.launch.py
```
Wait for it to settle — Gazebo window opens, spawner sequence finishes diff_cont → joint_broad → imu_broadcaster, one after another now

To confirm in a separate terminal
```
ros2 control list_controllers          # diff_cont must show "active". If any inactive, launch with ros2 run controller_manager spawner diff_cont
```
## Terminal 2 (SLAM, NAV2, Rviz)
```
source install/setup.bash && ros2 launch auracle_bringup slam_nav_rviz.launch.py use_sim_time:=true
```
## Terminal 3 (Teleop)
```
source install/setup.bash && ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r cmd_vel:=cmd_vel_joy
```

## Terminal 4 (Map saving)
This writes map_1.yaml + map_1.pgm into config/
```
source install/setup.bash
ros2 run nav2_map_server map_saver_cli -f ~/Documents/auracle_ws/src/auracle_bringup/config/map_1
```

## Localization + navigation
localization_neo_launch.py expects a neo_localization_node: block in its params_file (default config/nav2_params.yaml)

## Terminal 1 (hardware interface + lidar, or Gazebo)
```
source install/setup.bash && ros2 launch auracle_bringup launch_sim.launch.py
```

## Terminal 2 (Localization + Nav2 + RViz)
Once RViz is up: click "2D Pose Estimate" and click-drag on the map at the robot's actual start location/heading.
```
source install/setup.bash && ros2 launch auracle_bringup localization_nav_rviz.launch.py use_sim_time:=true   # or false for real robot
# to load a different map:
source install/setup.bash && ros2 launch auracle_bringup localization_nav_rviz.launch.py use_sim_time:=false map:=/path/to/other_map.yaml
```


# Real-time
## Flashing the arduino nano
```
#using arduino ide
# plug it into the Pi, confirm the port, upload
ls /dev/ttyUSB* /dev/ttyACM*

#using platformio
PlatformIO (from firmware/auracle_firmware/, where platformio.ini lives)
pio run -t upload
```

## Terminal 1 (hardware interface + lidar)
```
source install/setup.bash && ros2 launch auracle_bringup launch_robot.launch.py use_sim_time:=false
# if lidar port not at /dev/rplidar, override
source install/setup.bash && ros2 launch auracle_bringup launch_robot.launch.py use_sim_time:=false lidar_port:=/dev/ttyUSB0
```
## Terminal 2 (SLAM + Nav2 + RViz)
```
source install/setup.bash && ros2 launch auracle_bringup slam_nav_rviz.launch.py use_sim_time:=false
```
## Terminal 3 (teleop)
```
source install/setup.bash && ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r cmd_vel:=cmd_vel_joy
```

### namespacing
Add namespace:=<name> to all three commands, and give teleop the matching __ns remap
```
ros2 launch auracle_bringup launch_sim.launch.py namespace:=robot1
ros2 launch auracle_bringup slam_nav_rviz.launch.py use_sim_time:=true namespace:=robot1
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r cmd_vel:=cmd_vel_joy -r __ns:=/robot1
```
### Joystick (optional, either mode)
ros2 launch auracle_bringup joystick.launch.py use_sim_time:=<true|false> namespace:=<same as above>



