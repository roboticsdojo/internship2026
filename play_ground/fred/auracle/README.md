## How to test it
Gitclone the repo in your workspace src
Build the packages using the command: colcon build
Source your workspace
Run: ros2 launch auracle rsp.launch.py (This activates the robot state publisher node)
On a different terminal run the joint state publisher: ros2 run joint_state_publisher_gui joint_state_publisher_gui
Fire up rviz using the command: rviz2
In rviz under global options, select fixed frame as base-link
Add: tf and robot model
Under robot model, set the topic to robot state publisher


## Screenshots 
<img width="921" height="675" alt="image" src="https://github.com/user-attachments/assets/15cb9d85-347e-4864-ab95-6ec0343dd181" />


## Checklist
- [ ] `colcon build` passes
- [ ] `colcon test` passes
- [ ] No generated files committed
- [ ] Follows naming conventions in `robot_software/README.md`
- [ ] Package README updated if behaviour changed
