# robot_2_description

This package serves as the central physical definition storage layer for Robot 2. It houses the unified kinematic equations, model properties, hardware coordinate transform anchors, 3D meshes, and environmental gamefield world definitions.

---

## 📐 Kinematic and Visual Design Mapping

The underlying kinematic structure of Robot 2 is organized via a modular XML Macro architecture (`xacro`). This allows you to scale or modify subsystem properties without distorting parent transform calculations.

### Internal File Subsystems:
* `robot_2.urdf.xacro`: The master inclusion index processing all subsystem links.
* `robot_core.xacro`: Houses the structural link profiles, masses, physical geometries, and materials.
* `gazebo_control.xacro`: Implements the namespaced multi-wheel differential steering actuator drive plugins.
* `lidar.xacro`: Configures the virtual ray sensor properties and GPU-accelerated frame outputs.
* `inertial_macro.xacro`: Houses pure math matrix equations for box, cylinder, and sphere moment calculation properties.
