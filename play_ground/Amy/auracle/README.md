# package creation -- within ros2_ws/src
ros2 pkg create --build-type ament_cmake auracle \
        --dependencies rclcpp rclpy \
        --license Apache-2.0 \
        --maintainer-name "Amy Kibara" \
        --maintainer-email "amy.kibara@gmail.com" \
        --description "Auracle robot"

# include directories in cmakelists
install(
  DIRECTORY launch description config worlds models
  DESTINATION share/${PROJECT_NAME}
)

# Build
cd ~/Documents/ros2_ws
colcon build --symlink-install --packages-select auracle && source install/setup.bash

# Launch rsp - linked to robot.urdf.xacro
ros2 launch auracle rsp.launch.py use_sim_time:=false use_ros2_control:=false

# Publish joint states
source ~/Documents/ros2_ws/install/setup.bash && ros2 run joint_state_publisher_gui joint_state_publisher_gui

# Launch rviz2
rviz2

# Within rviz
Set Fixed Frame (top left, Global Options) to base_link (or base_footprint if your urdf uses one).
Click Add → RobotModel, and set its Description Topic to /robot_description.
Optionally Add → TF to see the frames.

# Save configuration in config files and launch from config (launch->jsp->default_rviz)
ros2 run rviz2 rviz2 -d ~/Documents/ros2_ws/src/auracle/config/default_rviz.rviz

# Create similar folder structure to one in auracle github and copy worlds and models
Add worlds and models folders

# Install dependencies
sudo apt update && sudo apt install ros-jazzy-ros-gz ros-jazzy-ros2-control ros-jazzy-ros2-controllers ros-jazzy-gz-ros2-control ros-jazzy-twist-mux ros-jazzy-twist-stamper

# Swap gazebo classic plugins for gazebo harmonic plugins in ros2_control.xacro and launch_sim.launch.py, and create yaml files within config folder
my_controllers.yaml — wheel geometry (separation: 0.297, radius: 0.033) is pulled straight from your gazebo_control.xacro. If your real robot's actual wheel measurements differ, update these — wrong values mean the robot drives but odometry will be off.
joystick.yaml — button/axis numbers assume a standard Xbox-layout gamepad (axis 1 = left stick vertical, axis 0 = left stick horizontal, button 4 = LB as the dead-man switch). If you're on keyboard-only or a different controller, this needs adjusting — let me know which and I'll fix the mapping.
twist_mux.yaml — routes both /cmd_vel_joy (your joystick) and /cmd_vel_tracker (from ball_tracker.launch.py) into the single output, joystick taking priority. No lock/e-stop topic configured — fine for now, just flagging it's empty.
gaz_ros2_ctl_use_sim.yaml -- for launching i think idk

# Build and launch
cd ~/Documents/ros2_ws
colcon build --packages-select auracle --symlink-install && source install/setup.bash
ros2 launch auracle launch_sim.launch.py

# Drive around using keyboard (oystick.launch.py is already included in launch_sim.launch.py, remapping teleop output to /cmd_vel_joy)
source install/setup.bash && ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r /cmd_vel:=/cmd_vel_joy

# Confirm publishing
ros2 topic echo /cmd_vel_joy 
ros2 topic echo /diff_cont/cmd_vel_unstamped

# Install mapping dependencies
sudo apt update && sudo apt install ros-jazzy-slam-toolbox ros-jazzy-navigation2 ros-jazzy-nav2-bringup 

# Copy required files from official nav2 packages
cp /opt/ros/jazzy/share/nav2_bringup/params/nav2_params.yaml \
   ~/Documents/ros2_ws/src/auracle/config/nav2_params.yaml

# Edit nav2_params.yaml and mapper_params_online_async
robot_base_frame → should match your URDF's base_link
robot_radius / footprint under the costmap configs → match your chassis dimensions
odom_topic → should be /diff_cont/odom (your diff_drive_controller's output) unless you remap it
scan_topic in the costmap observation_sources/plugins → matches your lidar's topic (/scan based on lidar.xacro)
behavior_server block → should already say behavior_plugins (not recovery_plugins) in a current nav2_bringup copy, since you're pulling from Jazzy's package directly

# Build and launch the simualtion on gazebo and SLAM_toolbox alongside simulation
cd ~/Documents/ros2_ws
colcon build --packages-select auracle --symlink-install 
source install/setup.bash && ros2 launch auracle launch_sim.launch.py
source install/setup.bash && ros2 launch auracle online_async_launch.py use_sim_time:=true

# Confirm scan and map topic exists for mapping and it publishes data
ros2 topic list


# Open rviz 
rviz2

Set Fixed Frame (top left, Global Options) to map
Click Add → By topic → find /map → add the Map display
Optionally add LaserScan on /scan and TF to watch it build live

# Drive around robot with teleop and fill map on rviz
source install/setup.bash && ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r /cmd_vel:=/cmd_vel_joy

# Save the map via CLI - saves to home folder, navigate and move to workspace
ros2 run nav2_map_server map_saver_cli -f ~/map_1
mv ~/map_1.* .

# Improve map quality
Set min_laser_range in mapper_param_online_async.yaml to 0.4 from 0.0, minimum_travel_distance/heading: 0.5 vs. your 0.2, and a bigger scan_buffer_size (30 vs 10) with link_scan_maximum_distance: 0.5 vs your 1.5
Wheel calibration: your wheel_separation: 0.22 / wheel_radius: 0.033 in my_controllers.yaml should be double-checked against the actual physical robot with a tape measure/caliper — a few-mm error here directly causes rotational drift of exactly the kind you're seeing, and it's free to fix.
launch_robot.launch.py sets use_sim_time: 'false' for rsp but hardcodes {'use_sim_time': True} on twist_mux and twist_stamper even in the real-robot launch. Probably copy-paste leftover from sim — worth cleaning up so all nodes agree on clock source.