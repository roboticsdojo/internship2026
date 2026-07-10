# Mars Rover Power Telemetry Simulation 

**Project:** Cars4Mars African Rover Challenge  
**Task:** Robotics Dojo RDI-6 (Create a Python Node)  
**Module:** Power Systems & Safety Telemetry  

## Overview
As part of developing the Robotics Dojo Learning, I came up with this idea to help me in learning as well as understanding nodes much easier.  

This package contains two nodes:
1. **`battery_publisher`**: Simulates a 3S LiPo battery starting at 12.6V and slowly draining as the rover's drive motors run.
2. **`battery_subscriber`**: Listens to the voltage broadcasts and triggers a critical system warning if the power drops below the safe return-to-base threshold (11.5V).

## Workspace Architecture
This package was built in a dedicated test workspace using the following structure:

```text
src/
└── rover_power_system/          
    ├── package.xml              
    ├── setup.py                 
    ├── README.md
    ├── resource/
    │   └── rover_power_system   <-- Required empty marker file..the file's contents do not matter at all—only its name and its existence matter              
    └── rover_power_system/      
        ├── __init__.py          
        ├── battery_publisher.py 
        └── battery_subscriber.py

# Woke up the base ROS 2 installation from the shortcut i created during installation
>load_ros 

# Created the workspace and source folders, then moved inside
>mkdir -p ~/test_ws/src
>cd ~/test_ws

# Ran an initial build to generate the core architecture (install, build, log folders)
colcon build

Step 2: Generating the Package Architecture

Instead of creating folders manually, I used the ROS 2 package generator to build the skeleton for my Python project, complete with the necessary dependencies (rclpy for Python, std_msgs for numbers).

Bash
>cd ~/test_ws/src
>ros2 pkg create --build-type ament_python rover_power_system --dependencies rclpy std_msgs

Step 3: Writing the Nodes

I opened the workspace in VS Code (code .). Inside the newly generated package, I navigated down into the inner module folder:
src/rover_power_system/rover_power_system/

Here, I created two files:

    battery_publisher.py (The broadcaster)

    battery_subscriber.py (The listener)

(The full Python code for these nodes is available in the repository files).
Step 4: Registering the Executables

For ROS 2 to recognize my Python scripts as runnable commands in the terminal, I had to map them in the setup.py file.

I updated the entry_points dictionary at the bottom of the file to look like this:
Python

    'entry_points': {
       'console_scripts': [
        'battery_publisher = rover_power_system.battery_publisher:main',
        'battery_subscriber = rover_power_system.battery_subscriber:main'
    ],
    },

Step 5: The Final Build and Launch

With the code written and registered, I compiled the package and sourced the environment so my terminal could find the new executables.

Bash
>cd ~/test_ws
>colcon build
>source install/setup.bash

To see the data flow, I split my terminal into two panes.

Terminal Pane 1 (The Listener):

Bash
>source install/setup.bash
>ros2 run rover_power_system battery_subscriber

Terminal Pane 2 (The Broadcaster):

Bash
>source install/setup.bash
>ros2 run rover_power_system battery_publisher

Immediately, the publisher started dropping the voltage, and the subscriber caught the drop, throwing a yellow CRITICAL warning in the terminal once it dipped below 11.5V!


Debugging & Troubleshooting Log

Building this wasn't perfectly smooth. Here are the main roadblocks I hit and how I solved them—hopefully, this saves someone else a few hours of debugging!
Bug 1: Package 'rover_power_system' not found

    What happened: I successfully ran the publisher in my first terminal pane. I split the terminal to run the subscriber, but ROS threw this error.
    (<rover_powerSystem_notfound.png>)

    The Fix: I forgot the "Multi-Terminal Trap." In Ubuntu, every terminal pane has its own isolated brain. Even though I built the workspace, the new pane had no idea it existed. I just had to run 
    >source install/setup.bash 
    in the new pane to wake it up.

Bug 2: No executable found

    What happened: The compiler found my package, but couldn't find the scripts to run.

    The Fix (Two parts):

        The Typo: I accidentally named my inner Python folder rove_power_system (missing the 'r'). Because setup.py was explicitly looking for rover_power_system, the link was broken. I renamed the folder.

        Unsaved Files: I was trying to compile while I still had unsaved changes in VS Code (the white dot on the file tabs). Always File -> Save All before running colcon build!

Bug 3: error: can't copy 'resource/rover_power_system': doesn't exist

    What happened: During a rebuild, the compiler crashed with this error message.

    The Fix: For ROS 2 to officially register a package, it requires a completely blank marker file sitting inside the resource/ folder, named exactly after the package. Somehow, mine got deleted. I fixed it instantly by creating a new blank file via the terminal:
    Bash

    >touch ~/test_ws/src/rover_power_system/resource/rover_power_system

Bug 4: The Ghost of "Hello World"

    What happened: I ran my newly written battery publisher, but my terminal started printing out Publishing: "Hello World: 41" from an old tutorial I did.

    The Fix: The Cache Trap! I had updated my Python code, but the compiler was still using old cache files from my previous build. To force a completely clean build, I wiped the factory floor before compiling again:
    Bash

    >cd ~/test_ws
    >rm -rf build/ install/ log/
    >colcon build