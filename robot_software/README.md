# robot_software 

##  Guideline

This directory contains two independent ROS2 workspaces — one per robot. Read this document fully before creating a package, writing a node, or naming anything.

CONSISTENCY HERE IS NOT OPTIONAL!!
What we want to avoid:

 - Inconsistently named topics
 - Malformed package structures
 - Undocumented parameters
 - Mediocre work



----------

## Workspace structure

Each workspace follows the same internal layout:

```
robot_X_ws/
└── src/
    ├── robot_X_bringup/       # Launch files that start the full robot
    ├── robot_X_description/   # URDF, meshes, tf2 static transforms
    ├── robot_X_navigation/    # Nav2 config, costmap params, planner tuning
    ├── robot_X_perception/    # Camera, LiDAR, sensor fusion nodes
    └── robot_X_<subsystem>/   # Add subsystems as needed, follow this pattern

```


----------

## Naming conventions

### Packages

All packages are prefixed with the robot identifier. Snake case throughout. No abbreviations unless they are industry-standard (e.g. `imu`, `lidar`).

```
robot_1_bringup
robot_1_perception
robot_2_navigation

```

Never name a package after a person, a temporary concept, or a sprint goal. Package names should describe what the package does, not when or why it was written.

### Nodes

Node names match the package they belong to, with a descriptive suffix for the specific function.

```
robot_1_lidar_processor
robot_1_waypoint_manager
robot_2_camera_detector

```

### Topics

Topic names follow the ROS2 convention: robot namespace first, then the logical group, then the signal name.

```
/robot_1/scan
/robot_1/odom
/robot_1/camera/image_raw
/robot_1/camera/detections
/robot_2/cmd_vel
/robot_2/imu/data

```

Always include the robot namespace prefix. When both robots are powered on simultaneously in the lab — which will happen — un-namespaced topics collide and both robots receive each other's commands. This has caused real accidents in past competitions.

### Services and actions

Follow the same namespace pattern:

```
/robot_1/set_mode        # service
/robot_1/navigate_to     # action

```

### Parameters

Parameters are declared in code and documented in a YAML file inside the package's `config/` directory. Never use magic numbers hardcoded in source files. If a value might ever need tuning — a speed limit, an inflation radius, a detection threshold — it is a parameter.

Parameter names are snake case and self-describing:

```yaml
# Good
max_linear_velocity: 0.5
obstacle_inflation_radius: 0.3
detection_confidence_threshold: 0.75

# Bad
v: 0.5
r: 0.3
thresh: 0.75

```

### Files and directories



Python nodes

`snake_case.py`

`lidar_processor.py`

C++ nodes

`snake_case.cpp`

`waypoint_manager.cpp`

Launch files

`snake_case.launch.py`

`full_robot.launch.py`

Config / params

`snake_case.yaml`

`nav2_params.yaml`

URDF

`robot_2.urdf.xacro`

`robot_1.urdf.xacro`

----------

## Package structure

Every package must follow this internal layout. `colcon` requires `package.xml` and `CMakeLists.txt` (C++) or `setup.py` (Python) — the rest is convention but expected.

```
robot_X_<subsystem>/
├── config/                  # Parameter YAML files
├── launch/                  # Launch files
├── robot_X_<subsystem>/     # Python module (same name as package)
│   ├── __init__.py
│   └── node_name.py
├── test/                    # Unit and integration tests
├── package.xml
├── setup.py                 # Python packages
└── README.md                # What this package does and how to run it

```

Every package must have its own `README.md`. It does not need to be long — a paragraph describing the package's responsibility, the topics it publishes and subscribes to, and the command to launch it is sufficient. If someone cannot understand what your package does, the README needs work.

----------

## ROS2 best practices

### Always declare parameters explicitly

```python
# Good — parameter is declared with a type and default
self.declare_parameter('max_speed', 0.5)
max_speed = self.get_parameter('max_speed').value

# Bad — hardcoded value buried in logic
cmd.linear.x = 0.5

```

### Use lifecycle nodes for any node that manages hardware

Sensor drivers, actuator controllers, and anything that connects to physical hardware should be implemented as lifecycle nodes. This lets the bringup system bring hardware up and down in a controlled order and recover from failures without restarting the entire ROS graph.

### Namespacing in launch files

All nodes launched for a robot must be given the robot's namespace. Do this at the launch file level, not inside the node code.

```python
# In your launch file
Node(
    package='robot_1_perception',
    executable='lidar_processor',
    namespace='robot_1',
    name='lidar_processor',
)

```

### QoS profiles must be explicit

Mismatched QoS settings between a publisher and subscriber produce silent failures — the topic appears in `ros2 topic list` but no messages arrive. Define QoS profiles explicitly wherever sensor data is involved.

```python
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

sensor_qos = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    durability=DurabilityPolicy.VOLATILE,
    depth=10
)
self.create_subscription(LaserScan, 'scan', self.scan_callback, sensor_qos)

```

For sensor data: `BEST_EFFORT` + `VOLATILE`. For state and transforms: `RELIABLE` + `TRANSIENT_LOCAL`.

### tf2 transforms

All transforms must go through `tf2`. Never manually compute a robot's world position from odometry outside of the tf2 tree. The transform tree for each robot should be:

```
map → odom → base_link → [sensor frames]

```

Verify your tree is complete and acyclic with:

```bash
ros2 run tf2_tools view_frames

```

A broken tf2 tree is one of the most common causes of Nav2 failures. Check it before debugging the planner.

### Never `rospy.sleep()` or `time.sleep()` inside a node

Use ROS2 timers instead. Blocking sleeps freeze the executor and prevent other callbacks from running.

```python
# Good
self.timer = self.create_timer(0.1, self.timer_callback)

# Bad
while True:
    self.do_something()
    time.sleep(0.1)

```

----------

## Testing

Every package with logic (not just launch files or pure config) must have at least one test. Place tests in the package's `test/` directory. Run all tests before opening a pull request:

```bash
colcon test --packages-select robot_1_perception
colcon test-result --verbose

```

Untested packages that break integration are the most common source of wasted sprint time. A single test that checks your node starts up without errors is better than no test at all. (We'll have an internal training on this)

----------

## Running the robots

```bash
# Launch robot 1 full stack
cd robot_software/robot_1_ws
source install/setup.bash
ros2 launch robot_1_bringup full_robot.launch.py

# Launch robot 2 full stack
cd robot_software/robot_2_ws
source install/setup.bash
ros2 launch robot_2_bringup full_robot.launch.py

```

Both robots can run simultaneously on the same network without conflict, provided namespacing conventions above have been followed.

----------

## Before opening a pull request

-   [ ] Package has a `README.md`
-   [ ] All parameters are declared and documented in `config/`
-   [ ] No hardcoded values in source files
-   [ ] Topics follow the `/robot_X/...` namespace convention
-   [ ] `colcon build` passes with no warnings
-   [ ] `colcon test` passes
-   [ ] `build/`, `install/`, `log/` are not staged in git