# Spawning the Robot
The spawn happens via a ros_gz_sim create node (not a separate service call) — it pulls the URDF straight from the robot_description topic:
```
spawn_entity = Node(
    package='ros_gz_sim',
    executable='create',
    arguments=[
        '-topic', 'robot_description',
        '-name', 'my_bot',
        '-x', '-1.1',
        '-y', '1.8',
        '-z', '0.08',
        '-R', '0.0',
        '-P', '0.0',
        '-Y', '1.57'
    ],
    namespace=namespace,
    output='screen'
)
```
Starting pose: x=-1.1, y=1.8, z=0.08, roll=0, pitch=0, yaw=1.57 rad (~90°). Entity name: my_bot
The world spawns into:
```
world_path = os.path.join(description_pkg, 'worlds', 'gamefield.world')
```
A launch file (launch_sim.launch.py )