# ROS 2 Workspace: Python Node and C++ Package Setup

This project contains:
- A ROS 2 Python node (`my_py_node`)
- A ROS 2 C++ package (`my_cpp_node`)
- Workspace built using `colcon`

---

#1. Create a ROS 2 Workspace

A ROS 2 workspace is a directory where ROS packages are stored and built.

mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

Workspace structure:

ros2_ws/
├── src/
├── build/
├── install/
└── log/

The src folder stores ROS packages.


#  2. C++ Package (my_cpp_node)

## Create package
```bash
cd ~/ros2_ws/src
ros2 pkg create my_cpp_node --build-type ament_cmake --dependencies rclcpp

## Add C++ node
my_cpp_node/src/my_node.cpp

-nano my_node.cpp
```bash
#include "rclcpp/rclcpp.hpp"

class MyNode : public rclcpp::Node
{
public:
    MyNode() : Node("my_cpp_node")
    {
        RCLCPP_INFO(this->get_logger(), "Hello ROS 2 from C++!");
    }
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MyNode>());
    rclcpp::shutdown();
    return 0;
}


##  Update CMakeLists.txt
```bash
cmake_minimum_required(VERSION 3.8)
project(my_cpp_node)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

add_executable(my_node src/my_node.cpp)

ament_target_dependencies(my_node rclcpp)

install(TARGETS
  my_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()

## Build
cd ~/ros2_ws
colcon build --symlink-install
source install/setup.bash

## Run
ros2 run my_cpp_node my_node

-Output :[INFO] [my_cpp_node]: Hello ROS 2 from C++! 

### -N/B: (ROS 2 Jazzy)
 Do NOT do:
Duplicate project() in CMakeLists.txt
Duplicate ament_package()
Repeat find_package() multiple times
Break CMake order

These cause build errors 


# 3. Python Node (my_py_node)

## Create package
```bash
ros2 pkg create my_py_node --build-type ament_python --dependencies rclpy

- This creates:

ros2_ws/src/my_py_node/  
├── package.xml  
├── setup.cfg  
├── setup.py  
├── resource/  
│   └── my_robot_package  
└── my_py_node/  
    └── __init__.py  
       my_node.py 
      __pycache__
       

## Node file (my_node.py)
```bash
import rclpy
from rclpy.node import Node

class MyNode(Node):
    def __init__(self):
        super().__init__('my_node')
        self.get_logger().info("Hello ROS 2 from Python!")

def main(args=None):
    rclpy.init(args=args)
    node = MyNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()

## Edit code in 'setup.py'
```bash
entry_points={
    'console_scripts': [
        'my_node = my_py_node.my_node:main',
    ],
},

## Update 'setup.cfg'
```bash
[develop]
script_dir=$base/lib/my_py_node

[install]
install_scripts=$base/lib/my_py_node

## Build Workspace
```bash
cd ~/ros2_ws
colcon build --symlink-install
source install/setup.bash


-Run
```bash
ros2 run my_py_node my_node

-Output:  [INFO] [my_node]: Hello ROS 2 from Python!
