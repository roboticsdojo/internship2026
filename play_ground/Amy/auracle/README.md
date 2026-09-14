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
sudo apt update && sudo apt install ros-jazzy-slam-toolbox ros-jazzy-navigation2 ros-jazzy-nav2-bringup libserial-dev

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
cd ~/Documents/ros2_ws && colcon build --packages-select auracle --symlink-install 
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
ros2 run nav2_map_server map_saver_cli -f ~/map_5
mv ~/map_5.* .

