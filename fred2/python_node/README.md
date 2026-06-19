# PY_PUBSUB

## Description
This package creates a python node and subscriber connected by a topic called "topic". 

## Dependencies
- ROS 2 Jazzy
- rclpy / rclcpp

## How to Build
The guide followed is https://docs.ros.org/en/jazzy/Tutorials/Beginner-Client-Libraries/Writing-A-Simple-Py-Publisher-And-Subscriber.html

The package was first created 
ros2 pkg create --build-type ament_python --license Apache-2.0 py_pubsub

Both codes for the publisher and the subscriber were imported to the package from github using the following commands
wget https://raw.githubusercontent.com/ros2/examples/jazzy/rclpy/topics/minimal_publisher/examples_rclpy_minimal_publisher/publisher_member_function.py

wget https://raw.githubusercontent.com/ros2/examples/jazzy/rclpy/topics/minimal_subscriber/examples_rclpy_minimal_subscriber/subscriber_member_function.py

Some dependencied were added in the setup.py and package.xml files
<exec_depend>rclpy</exec_depend>
<exec_depend>std_msgs</exec_depend>
 
 and an entry point added 
 entry_points={
        'console_scripts': [
                'talker = py_pubsub.publisher_member_function:main',
                'listener = py_pubsub.subscriber_member_function:main',
        ],
},

```bash
cd ~/ros2_ws
colcon build --packages-select py_pubsub
source install/setup.bash
```

## How to Run
```bash
ros2 run py_pubsub talker
```