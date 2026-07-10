#C++ Node Communication Simulator 
#Task: Robotics Dojo RDI-7 (Create a C++ Node)
#Module: Core ROS 2 Communication

Overview

As part of the Robotics Dojo Learning progression, I transitioned from Python (rclpy) to C++ (rclcpp) to understand how ROS 2 handles compiled languages. To keep things isolated and focus entirely on C++ syntax and the CMake build system, I built a standalone communication package.

This package contains two C++ nodes:

    dojo_pub: A publisher that broadcasts a continuous text stream (std_msgs/String) counting the active broadcast ticks.

    dojo_sub: A subscriber that listens to the dojo_comms topic and logs the exact message payload to the terminal.

Workspace Architecture

Unlike Python, C++ packages in ROS 2 do not require nested module folders or empty marker files. All raw code sits directly in the src directory, and compilation is handled by CMake.

Task/Project structure:

src/
└── dojo_cpp_nodes/          
    ├── package.xml              
    ├── CMakeLists.txt           <-- The master C++ build instructions
    ├── README.md             
    └── src/      
        ├── publisher_node.cpp 
        └── subscriber_node.cpp

Step-by-Step Build Instructions
Step 1: Generating the Package Architecture

I generated a new package specifically set up for C++ using the ament_cmake build type and pulling in the C++ client library (rclcpp).
Bash

> cd ~/test_ws/src
> ros2 pkg create --build-type ament_cmake dojo_cpp_nodes --dependencies rclcpp std_msgs

Step 2: Writing the Nodes

Inside the src/dojo_cpp_nodes/src/ folder, I created two C++ files:

    publisher_node.cpp

    subscriber_node.cpp

(The full source code demonstrating C++ class inheritance, strict variable typing, and pointer usage is available in the repository files).
Step 3: Registering the Executables in CMakeLists

For the C++ compiler to translate the text files into runnable machine code, I had to update the CMakeLists.txt file. I added the following blocks below the dependency declarations:
CMake

# 1. Build the publisher executable
add_executable(dojo_pub src/publisher_node.cpp)
ament_target_dependencies(dojo_pub rclcpp std_msgs)

# 2. Build the subscriber executable
add_executable(dojo_sub src/subscriber_node.cpp)
ament_target_dependencies(dojo_sub rclcpp std_msgs)

# 3. Install them to the terminal
install(TARGETS
  dojo_pub
  dojo_sub
  DESTINATION lib/${PROJECT_NAME}
)

Step 4: The Build and Launch

With the C++ code written and the CMake instructions set, I compiled strictly this new package to save time.
Bash

> cd ~/test_ws
> colcon build --packages-select dojo_cpp_nodes

To see the data flow, I split my terminal into two panes.

Terminal Pane 1 (The Listener):
Bash

> source install/setup.bash
> ros2 run dojo_cpp_nodes dojo_sub

Terminal Pane 2 (The Broadcaster):
Bash

> source install/setup.bash
> ros2 run dojo_cpp_nodes dojo_pub

Immediately, the C++ publisher started broadcasting the active count, and the subscriber echoed it back in real-time

Debugging & Troubleshooting Log

Building in C++ introduces new compiler strictness. Here are the main roadblocks I hit and how I solved them.
Bug 1: The 0.08s "Ghost Build" (No Executable Found)

    What happened: I ran colcon build, and it finished in 0.08 seconds. When I tried to ros2 run the node, the terminal said "No executable found."

    The Fix: The compiler read my files, saw no saved changes on the hard drive, and skipped the build entirely. I had unsaved changes in VS Code (the "White Dot of Doom" on the file tab). Pressing Ctrl + S to save all files before running colcon build fixed it.

Bug 2: Missing the install Block

    What happened: Even after saving the files, the node still wouldn't run.

    The Fix: In C++, building the code and installing the code are two different steps. I had told CMake to build the executable (add_executable), but I forgot the install(TARGETS...) block. Without that block, the executables are thrown away after compiling instead of being pushed to the ROS 2 library where the terminal can find them.

Bug 3: Forcing a Clean Rebuild

    What happened: After fixing my CMakeLists.txt, the compiler was still acting up based on old cached instructions.

    The Fix: Sweeping the factory floor. I deleted the cache folders to force the C++ compiler to read the instructions entirely from scratch.
    Bash
> cd ~/test_ws
> rm -rf build/ install/ log/
> colcon build --packages-select dojo_cpp_nodes