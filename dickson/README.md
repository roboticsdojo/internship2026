# ROS 2 Publisher and Subscriber Nodes

This README describes the steps involved in creating simple ROS 2 publisher and subscriber nodes in both Python and C++. The project can be used for learning purposes.

## Contents

* Python publisher node
* Python subscriber node
* C++ publisher node
* C++ subscriber node

---

# Creating the Python Publisher and Subscriber Nodes

Open the Ubuntu 24.04 terminal and run:

```bash
code .
```

This opens Visual Studio Code.

## Step 1: Install Colcon

Run the commands separately:

```bash
sudo apt install colcon
sudo apt install python3-colcon-common-extensions
```

## Step 2: Create a Workspace

```bash
mkdir -p ~/ros2_ws/src
```

## Step 3: Navigate to the Workspace

```bash
cd ~/ros2_ws
```

## Step 4: Create a Python Package

```bash
ros2 pkg create --build-type ament_python my_py_pkg
```

## Step 5: Move to the Package Directory

```bash
cd ~/ros2_ws/src/my_py_pkg/my_py_pkg
```

## Step 6: Create the Publisher and Subscriber Files

```bash
touch py_pub.py my_sub.py
```

Write the publisher code in `py_pub.py` and the subscriber code in `my_sub.py`.

## Step 7: Edit `setup.py`

Go back to the package root:

```bash
cd ~/ros2_ws/src/my_py_pkg
```

Open the file:

```bash
code setup.py
```

Locate:

```python
entry_points={
    'console_scripts': [],
},
```

Replace it with:

```python
entry_points={
    'console_scripts': [
        'py_pub = my_py_pkg.py_pub:main',
        'my_sub = my_py_pkg.my_sub:main',
    ],
},
```

## Step 8: Edit `package.xml`

Open:

```bash
code package.xml
```

Locate:

```xml
<test_depend>ament_copyright</test_depend>
<test_depend>ament_flake8</test_depend>
<test_depend>ament_pep257</test_depend>
<test_depend>python3-pytest</test_depend>
```

Add the following above the test dependencies:

```xml
<depend>rclpy</depend>
<depend>std_msgs</depend>
```

## Step 9: Build the Workspace

Go to the workspace root:

```bash
cd ~/ros2_ws
```

Build:

```bash
colcon build
```

## Step 10: Source the Workspace

> **Note:** Every time you open a new terminal, source the workspace:

```bash
source ~/ros2_ws/install/setup.bash
```

For now, run:

```bash
source install/setup.bash
```

## Step 11: Run the Publisher

Open Terminal 1:

```bash
source ~/ros2_ws/install/setup.bash
ros2 run my_py_pkg py_pub
```

Expected output:

```text
Publishing: "Hello 0"
Publishing: "Hello 1"
Publishing: "Hello 2"
...
```

## Step 12: Run the Subscriber

Open Terminal 2:

```bash
source ~/ros2_ws/install/setup.bash
ros2 run my_py_pkg my_sub
```

Expected output:

```text
I heard: "Hello 0"
I heard: "Hello 1"
I heard: "Hello 2"
...
```

---

# Creating the C++ Publisher and Subscriber Nodes

## Step 1: Go to the Workspace Source Directory

```bash
cd ~/ros2_ws/src
```

## Step 2: Create the C++ Package

```bash
ros2 pkg create --build-type ament_cmake my_cpp_pkg --dependencies rclcpp std_msgs
```

This automatically adds the required dependencies.

## Step 3: Create the Source Files

Move to the source directory:

```bash
cd my_cpp_pkg/src
```

Create the files:

```bash
touch cpp_pub.cpp cpp_sub.cpp
```

## Step 4: Write the Publisher Code

Open:

```bash
code cpp_pub.cpp
```

Write the C++ publisher code.

## Step 5: Write the Subscriber Code

Open:

```bash
code cpp_sub.cpp
```

Write the C++ subscriber code.

## Step 6: Edit `CMakeLists.txt`

Go back to the package root:

```bash
cd ~/ros2_ws/src/my_cpp_pkg
```

Locate:

```cmake
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
```

Before:

```cmake
ament_package()
```

Add:

```cmake
add_executable(cpp_pub src/cpp_pub.cpp)
ament_target_dependencies(cpp_pub rclcpp std_msgs)

add_executable(cpp_sub src/cpp_sub.cpp)
ament_target_dependencies(cpp_sub rclcpp std_msgs)

install(TARGETS
  cpp_pub
  cpp_sub
  DESTINATION lib/${PROJECT_NAME}
)
```

Save the file.

## Step 7: Build the Workspace

Go to the workspace root:

```bash
cd ~/ros2_ws
```

Build:

```bash
colcon build
```

## Step 8: Source the Workspace

```bash
source ~/ros2_ws/install/setup.bash
```

## Step 9: Run the Publisher

Terminal 1:

```bash
ros2 run my_cpp_pkg cpp_pub
```

Expected output:

```text
[INFO] Publishing: 'Hello 0'
[INFO] Publishing: 'Hello 1'
...
```

## Step 10: Run the Subscriber

Terminal 2:

```bash
ros2 run my_cpp_pkg cpp_sub
```

Expected output:

```text
[INFO] I heard: 'Hello 0'
[INFO] I heard: 'Hello 1'
...
```

---

# Conclusion

You have successfully created simple ROS 2 publisher and subscriber nodes in both Python and C++.

thats is it Dickson
