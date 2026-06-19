# ROS 2 Workspace: Python Node and C++ Package Setup

This project contains:
- A ROS 2 Python node (`my_py_node`)
- A ROS 2 C++ package (`my_cpp_node`)
- Workspace built using `colcon`



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








# ROS 2 Publisher–Subscriber Demo (Python + C++)

This project demonstrates a basic ROS 2 communication system using:
- A **publisher node**
- A **subscriber node**

Implemented in:
- Python (`my_py_node`)
- C++ (`my_cpp_node`)


## Python Package(my_py_node)
### Update the setup.py
```bash
from setuptools import find_packages, setup

package_name = 'my_py_node'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='jacque',
    maintainer_email='jacque@email.com',
    description='ROS2 Python pub-sub demo',
    license='Apache-2.0',

    entry_points={
        'console_scripts': [
            'publisher_node = my_py_node.publisher:main',
            'subscriber_node = my_py_node.subscriber:main',
        ],
    },
)



### Update publisher.py
```bash
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class PublisherNode(Node):
    def __init__(self):
        super().__init__('publisher_node')
        self.pub = self.create_publisher(String, 'topic', 10)
        self.timer = self.create_timer(1.0, self.publish_msg)

    def publish_msg(self):
        msg = String()
        msg.data = "Hello from publisher"
        self.pub.publish(msg)
        self.get_logger().info(f'Publishing: {msg.data}')

def main():
    rclpy.init()
    node = PublisherNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


### Update subsriber.py
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class PublisherNode(Node):
    def __init__(self):
        super().__init__('publisher_node')
        self.pub = self.create_publisher(String, 'topic', 10)
        self.timer = self.create_timer(1.0, self.publish_msg)

    def publish_msg(self):
        msg = String()
        msg.data = "Hello from publisher"
        self.pub.publish(msg)
        self.get_logger().info(f'Publishing: {msg.data}')

def main():
    rclpy.init()
    node = PublisherNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()



## C++ package (my_cpp_node)
### Update CMakeLists.txt
cmake_minimum_required(VERSION 3.8)
project(my_cpp_node)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(publisher_node src/publisher_node.cpp)
ament_target_dependencies(publisher_node rclcpp std_msgs)

add_executable(subscriber_node src/subscriber_node.cpp)
ament_target_dependencies(subscriber_node rclcpp std_msgs)

install(TARGETS
  publisher_node
  subscriber_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()


### Update publisher_node.cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using std::placeholders::_1;

class PublisherNode : public rclcpp::Node
{
public:
    PublisherNode() : Node("publisher_node")
    {
        pub_ = this->create_publisher<std_msgs::msg::String>("topic", 10);
        timer_ = this->create_wall_timer(
            std::chrono::seconds(1),
            std::bind(&PublisherNode::publish_msg, this));
    }

private:
    void publish_msg()
    {
        std_msgs::msg::String msg;
        msg.data = "Hello from C++ publisher";
        pub_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "Publishing: %s", msg.data.c_str());
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<PublisherNode>());
    rclcpp::shutdown();
    return 0;
}


### Update subscriber_node.cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class SubscriberNode : public rclcpp::Node
{
public:
    SubscriberNode() : Node("subscriber_node")
    {
        sub_ = this->create_subscription<std_msgs::msg::String>(
            "topic",
            10,
            std::bind(&SubscriberNode::callback, this, std::placeholders::_1));
    }

private:
    void callback(const std_msgs::msg::String::SharedPtr msg)
    {
        RCLCPP_INFO(this->get_logger(), "Received: %s", msg->data.c_str());
    }

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SubscriberNode>());
    rclcpp::shutdown();
    return 0;
}




-Terminal 1
ros2 run my_py_node subscriber
# OR
ros2 run my_cpp_node subscriber_node


-Terminal 2
ros2 run my_py_node publisher

# OR
ros2 run my_cpp_node publisher_node
