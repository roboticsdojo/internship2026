# pub_sub_pack_cpp

A minimal ROS 2 C++ package implementing a basic talker (publisher) and listener (subscriber) pattern.

## Overview

- **Publisher node(min_cpp_pub)** Periodically publishes a string message to the /greetings_cpp topic.
- **Subscriber node(min_cpp_sub)** Listens to the /greetings_cpp topic and logs the received messages to the console.

### Prerequisites

- **ROS2 Jazzy** (or compatible version) 
- `colcon` build tool
- `std_msgs` and `rclcpp` packages

### Building the Package

1. In a new terminal, navigate to your ROS 2 workspace (usually ros2_ws).
   ```
   cd ~/ros2_ws/src
   ```
2. Clone/add the package from the Github repository
   
3. After cloning, go back to the root of the workspace (~/ros2_ws) to install the dependencies::
   ```
   cd ~/ros2_ws
   rosdep install -i --from-path src --rosdistro $ROS_DISTRO -y
   ```
4. Build the package:
   ```
   colcon build --packages-select pub_sub_pack_cpp
   ```

## Running the Nodes
To run the nodes, you need two terminal sessions (or two split panes). Important: You must source the setup file in every new terminal or split you open.
 
### Terminal 1: Publisher
```
source ~/ros2_ws/install/setup.bash
ros2 run pub_sub_pack_cpp min_cpp_pub
```
### Terminal 2: Subscriber
```
source ~/ros2_ws/install/setup.bash
ros2 run pub_sub_pack_cpp min_cpp_sub
```
To terminate the process press `Ctrl` + `C` (or `Cmd` + `C` in Mac)

## Testing and Linting
The CMakeLists.txt and package.xml are configured with ament_lint_auto. These tools check your code for style, copyright headers, and best practices. **Note on Skipped Tests:** The strict automated copyright (`ament_cmake_copyright`), formatting (`uncrustify`), and style (`cpplint`) checks have been explicitly bypassed in `CMakeLists.txt`. This allows the package to pass its test suite while preserving custom source file header documentation and styling.

To run the tests/linters:
```
colcon test --packages-select pub_sub_pack_cpp
```
To see the results:
```
colcon test-result --all
```


## License

Distributed under the Apache 2.0 License. See the `LICENSE` file in this repository for the full terms and conditions.
