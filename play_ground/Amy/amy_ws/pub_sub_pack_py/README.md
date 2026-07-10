# pub_sub_pack_py

A minimal ROS 2 Python package implementing a basic talker (publisher) and listener (subscriber) pattern.

## Overview

- **Publisher node(min_py_pub)**  Periodically publishes a string message to the /greetings_py topic.
- **Subscriber node(min_py_sub)** Listens to the /greetings_py topic and logs the received messages.

### Prerequisites

- **ROS2 Jazzy** (or compatible version) 
- `colcon` build tool
- `std_msgs` and `rclpy` packages

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
   colcon build --packages-select pub_sub_pack_py
   ```

## Running the Nodes
To run the nodes, you need two terminal sessions (or two split panes). You must source the setup file in every new terminal or split you open.
 
### Terminal 1: Publisher
```
source ~/ros2_ws/install/setup.bash
ros2 run pub_sub_pack_py min_py_pub
```
### Terminal 2: Subscriber
```
source ~/ros2_ws/install/setup.bash
ros2 run pub_sub_pack_py min_py_sub
```
To terminate the process press `Ctrl` + `C` (or `Cmd` + `C` in Mac)

## Testing and Linting
This package is configured with several Python linters:
- flake8: Checks for style guide enforcement.
- pep257: Checks for compliant docstrings.
- copyright: Checks for required license headers.
To prioritize custom file header formatting, documentation syntax, and inline commenting structures over pedantic styling rules, specific linting constraints (`flake8`, and `pep257`) have been explicitly bypassed or set to force-pass. This approach keeps the testing pipeline focused squarely on functional integrity and successful compilation.

To run the tests/linters:
```
colcon test --packages-select pub_sub_pack_py
```
To see the results:
```
colcon test-result --all
```


## License

Distributed under the Apache 2.0 License. See the [LICENSE](LICENSE) file in this repository for the full terms and conditions.
