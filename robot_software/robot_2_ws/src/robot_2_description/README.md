# robot_2_description

This package serves as the central physical definition storage layer for Robot 2. It houses the unified kinematic equations, model properties, hardware coordinate transform anchors, 3D meshes, and environmental gamefield world definitions.

---

## 📐 Kinematic and Visual Design Mapping

The underlying kinematic structure of Robot 2 is organized via a modular XML Macro architecture (`xacro`). This allows you to scale or modify subsystem properties without distorting parent transform calculations.

### Internal File Subsystems:
* `robot_2.urdf.xacro`: The master inclusion index processing all subsystem links. Selects between `ros2_control.xacro` and `gazebo_control.xacro` via the `use_ros2_control` xacro arg (default `true`).
* `robot_core.xacro`: Houses the structural link profiles, masses, physical geometries, and materials. Wheel collision geometry is cylindrical (matching the visual mesh) with explicit friction (`mu1`/`mu2`) set — sphere collision was tried initially but caused wheel slip and odometry drift on rotation due to its point-contact-only ground contact.
* `ros2_control.xacro`: **Active by default.** Declares the `ros2_control` hardware interfaces (velocity command, position/velocity state per wheel joint) and loads `gz_ros2_control::GazeboSimROS2ControlPlugin` with `robot_2_bringup/config/my_controllers.yaml`. tf frame IDs it produces are unprefixed (`base_footprint`, `odom`, etc.) to match `robot_state_publisher`'s output — see `tf_frame_prefix_enable: false` in that config.
* `gazebo_control.xacro`: Alternate drive path using Gazebo's native `DiffDrive`/`OdometryPublisher`/`JointStatePublisher` system plugins instead of `ros2_control`. Only active when launched with `use_ros2_control:=false`; not used by default.
* `lidar.xacro`: Configures the virtual ray sensor (`gpu_lidar`) properties and GPU-accelerated frame outputs. **Requires** the world to explicitly load the `gz::sim::systems::Sensors` system plugin (see `worlds/gamefield.world`) — without it, the sensor spawns but silently produces no data, since Gazebo's implicit default plugin set does not include `Sensors`.
* `inertial_macro.xacro`: Houses pure math matrix equations for box, cylinder, and sphere moment calculation properties.

---

## 🌍 World Definitions

* `worlds/gamefield.world`: The competition gamefield world. Explicitly declares `Physics`, `UserCommands`, `SceneBroadcaster`, and `Sensors` (`ogre2` render engine) system plugins — required once any plugin is declared, since doing so disables Gazebo's implicit default plugin set entirely. Includes the `gamefield` model at a configurable pose (currently rotated 270°/`4.7124` rad to match the physical field orientation).
* `worlds/models/gamefield/`: The gamefield model itself (meshes, `model.sdf`, `model.config`).