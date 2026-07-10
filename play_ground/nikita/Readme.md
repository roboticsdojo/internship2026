CREATING A WORKSPACE IN ROS2 JAZZY
---------------------------------------------

A ROS2 workspace is the development environment where you organize, build and test ROS2 Packages.
ROS2 packages are organizational units that allow you to easily install and share your code for others to use to build. 
In ROS2, packages are built using ament as the build system, and colcon is used to build them.

In order to create a workspace in ROS2 Jazzy, the prerequistes are:
 1) ROS2 must be installed
 2) colcon must also be installed (since its tools will be used to build the workspace)

To verify that colcon is installed run the command " colcon version-check" in your terminal

If it is not installed, install it using the command " sudo apt install python3-colcon-common-extensions" in your terminal.

A list of items being downloaded should appear in the terminal

************************image*************

There is the option to source the ros2 setup bash file either before or after creating the src folder.

In this instance, I sourced the bash file before creating the workspace folder

```bash
source /opt/ros/jazzy/setup.bash
```

To create the workspace folder run the command, 

```bash
mkdir -p ~/ros2_ws/src
```
This creates a folder called `ros2_ws` in the home directory. `src` is the folder that will hold all executable files and folders for the workspace. Which, in this case, will be the ros2 packages

Change directories to the `src` folder using the command:

```bash
cd ~/ros2_ws/src
```

I created the python package first , using the command

```bash
ros2 pkg create --build-type ament_python --license Apache-2.0 first_python_package
```

`ament_python` is one of the colcon build tools for building python packages
`Apache-2.0` is the name of the license that is assigned to this package. It allows anyone to use,modify, distribute and sell the software that is built using this code. 
`first_python_package` is the name of the package that will be created.

After executing that command,the terminal will show folders being created

List folders and files present in `src`. This shows the created python package folder called `first_python_package`.
Change directories to `first_python_package`.
```bash
cd first_python_package
```
Listing its contents shows a folder in the same name.
```bash
$ ls
LICENSE  first_python_package  package.xml  resource  setup.cfg  setup.py  test
```

Change directories to `first_python_package`  
```bash
cd first_python_package
```

I created a file called `publisher_node.py` using the command
```bash
touch publisher_node.py
```

I used the command below to make it executable
```bash
chmod +x publisher_node.py
```

I then opened the folder `first_python_package` in Visual Studio Code
```bash
code . 
```

For the C++ package, 

change directories to the `src` folder using the command
```bash
cd ../
```
(I did it twice to go back to the `src` folder)

I created the c++ package using the command below in the terminal
```bash
ros2 pkg create  --build-type ament_cmake --license Apache-2.0 first_cpp_package
```
It follows the same format as when the python package was created, but `ament_cmake` is used since it is a c++ package. `first_spp_package` is the name of the package.

For easier navigation between the 2 packages, I opened the src folder in vs code
```bash
code . 
```
In the terminal, I navigated to `src` folder in `first_cpp_package` and created a file called subscriber node.cpp
 ```bash
cd first_cpp_package
cd src
touch subscriber_node.cpp
 ```

CREATING A PYTHON PUBLISHER NODE IN ROS2 JAZZY
-----------------------------------------------------------------
In `publisher_node.py`,
```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
```
The `rclpy` library contains tools to build the python node such as the `Node` class. 
`std_msgs.msg import String` imports the message type that the node uses to structure the data it passes to the topic. In this case, the data is sent as a String of characters.

Define the class for our node
```python
class FirstPublisherNode(Node)
```
the custom class can be named anything but it must inherit properties from the inherited `Node` class that was imported.

Initialization function for the node
```python
 def __init__(self):
        super().__init__('first_publisher_node')
        self.publisher_ = self.create_publisher(String, 'first_topic', 10)
        timer_period= 0.5
        self.timer = self.create_timer(timer_period, self.timer_callback)
        self.i=0
```
the constructor initializes the class `FirstPublisherNode` with attributes of the `Node` class
`self.publisher` makes this node a publisher node with the attributes of the String data, name of the topic and queue period.
`timer_period` defines the time in seconds between consecutive execution of the callback function. Here, the callback function will be called every 0.5 seconds
`self.i` defines the counter

```python
def timer_callback(self):
        msg= String()
        msg.data = 'Hello World: %d' % self.i
        self.publisher_.publish(msg)
        self.get_logger().info('Publishing: "%s"' %msg.data)
        self.i +=1
```
the timer callback function publishes a message to the topic every 0.5 seconds.

```python
def main(args=None):
        rclpy.init(args=args)
        first_publisher_node= FirstPublisherNode()
        rclpy.spin(first_publisher_node)
        first_publisher_node.destroy_node()
        rclpy.shutdown()
```
the main function initializes the ROS 2 client library, creates an instance of the FirstPublisherNode class, and starts spinning the node to process incoming messages and execute the timer callback function.
When the node is shut down, it destroys the node and shuts down the ROS 2 client library.



CREATING A C++ SUBSCRIBER NODE IN ROS2 JAZZY
--------------------------------------------------------------
In `subscriber_node.cpp`,
First import the libraries 
```c++
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
```
Create the subscriber class which will inherit from the base class
```c++
class FirstSubscriber : public rclcpp::Node
```
This is the subscriber node that will listen to the messages on the topic

```c++
 public: 
     FirstSubscriber() : Node("first_subscriber")
     { 
        auto topic_callback = [this] (std_msgs::msg::String::UniquePtr msg) -> void {
            RCLCPP_INFO(this ->get_logger(), "I heard: '%s'", msg->data.c_str());
        };
        subscription_ = this-> create_subscription<std_msgs::msg::String>("first_topic", 10, topic_callback):
     }
```
`topic_callback` is the function that receives string message data over the topic and writes it to the console
`create_subscription` executes the callback function when a message is received on the topic

```c++
int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp:: spin(std::make_shared<FirstSubscriber>());
    rclcpp::shutdown();
    return 0;
}
```
The main function initializes the ROS 2 system, creates an instance of the FirstSubscriber node, and starts spinning to process incoming messages. 
It will continue to run until the node is shut down, at which point it will clean up and exit.

RUNNING THE PUBLISHER AND SUBSCRIBER NODES
-----------------------------------------------
Navigate to your wokspace folder
In this case the folder is `ros2_ws`
```bash
cd path/to/ros2_ws
```

Source the core ros2 environment. If it has already been permanently echoed into your terminal, this step can be skipped

```bash
source /opt/ros/jazzy/setup.bash
```

If you want to echo it permanently into your terminal so that you don't have to do it every time, use the command
```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
```

Next, source the local bash file
```bash
source install/setup.bash
```

Next run the python publisher node
```bash
ros2 run first_python_package talker
```

Then, run the c++ subscriber node

```bash
ros2 run first_cpp_package listener
```

The order in which they are run does not matter. The listener can be run first before the talker. It will still work.

Now, observe the published messages from the publisher, and the messages received by the subscriber node.

