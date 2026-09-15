import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription,
    DeclareLaunchArgument,
    AppendEnvironmentVariable,
    UnsetEnvironmentVariable,
    RegisterEventHandler,
)
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node


def generate_launch_description():

    # -----------------------------
    # Robot State Publisher
    # -----------------------------

    package_name = 'robot_2_bringup'

    use_ros2_control = LaunchConfiguration('use_ros2_control')

    robot_state_publisher = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [
                os.path.join(
                    get_package_share_directory("robot_2_bringup"),
                    "launch",
                    "robot_state_publisher.launch.py",
                )
            ]
        ),
        launch_arguments={
            'use_sim_time': 'true',
            'use_ros2_control': use_ros2_control,
            # NOTE: explicit here. ros2_control.xacro's sim_mode arg
            # defaults to 'false' (real robot) so that launch_robot.launch.py
            # doesn't need to pass anything extra. Sim must therefore set
            # this explicitly, or it would silently try to load the real
            # Arduino hardware interface instead of GazeboSimSystem.
            'sim_mode': 'true',
        }.items()
    )

    # -----------------------------
    # World
    # -----------------------------
    world = LaunchConfiguration("world")

    default_world_path = os.path.join(
        get_package_share_directory("robot_2_description"),
        "worlds",
        "gamefield.world",
    )

    world_arg = DeclareLaunchArgument(
        "world",
        default_value=default_world_path,
        description="World file",
    )

    # -----------------------------
    # Robot starting pose on the gamefield
    # -----------------------------
    spawn_x = LaunchConfiguration("spawn_x")
    spawn_y = LaunchConfiguration("spawn_y")
    spawn_z = LaunchConfiguration("spawn_z")
    spawn_yaw = LaunchConfiguration("spawn_yaw")

    spawn_x_arg = DeclareLaunchArgument("spawn_x", default_value="-1.18")
    spawn_y_arg = DeclareLaunchArgument("spawn_y", default_value="2.44")
    spawn_z_arg = DeclareLaunchArgument("spawn_z", default_value="0.05")
    spawn_yaw_arg = DeclareLaunchArgument("spawn_yaw", default_value="0.0992")

    # -----------------------------
    # Gazebo resource paths
    # -----------------------------
    gz_resources = AppendEnvironmentVariable(
        "GZ_SIM_RESOURCE_PATH",
        os.path.join(get_package_share_directory("robot_2_description")) + ":" +
        os.path.join(get_package_share_directory("robot_2_description"), "worlds", "models")
    )

    # -----------------------------
    # Gazebo Simulation Engine
    # -----------------------------
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [
                os.path.join(
                    get_package_share_directory("ros_gz_sim"),
                    "launch",
                    "gz_sim.launch.py",
                )
            ]
        ),
        launch_arguments={
            # Start Gazebo Harmonic server and GUI.
            "gz_args": ["-r -v4 ", world],
            #"gz_args": ["-r -s -v4 ", world],
            "on_exit_shutdown": "true",
        }.items(),
    )

    # -----------------------------
    # Spawn Robot Node
    # -----------------------------
    spawn_robot = Node(
        package="ros_gz_sim",
        executable="create",
        namespace="robot_2",
        arguments=['-topic', '/robot_2/robot_description',
                   '-name', 'robot_2',
                   '-x', spawn_x,
                   '-y', spawn_y,
                   '-z', spawn_z,
                   '-Y', spawn_yaw],
        output="screen",
    )

    # NOTE: standalone joint_state_publisher node removed. It publishes
    # default/zero joint positions on a timer and was competing with
    # joint_broad (joint_state_broadcaster), which publishes the *real*
    # joint states coming from Gazebo via GazeboSimSystem. With
    # use_ros2_control:true, joint_broad is the only thing that should
    # ever publish /robot_2/joint_states.

    # -----------------------------
    # ROS <-> Gazebo Bridge
    # -----------------------------
    bridge_config = os.path.join(
        get_package_share_directory("robot_2_bringup"),
        "config",
        "gz_bridge.yaml",
    )

    ros_gz_bridge = Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        namespace="robot_2",
        arguments=[
            "--ros-args",
            "-p",
            f"config_file:={bridge_config}",
        ],
        output="screen",
    )

    # -----------------------------
    # Hardware Controller Spawners
    # -----------------------------
    diff_drive_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "diff_cont",
            "-c", "/robot_2/controller_manager",
            "--controller-ros-args",
            # cmd_vel: diff_cont's real topic is /robot_2/diff_cont/cmd_vel
            "-r /robot_2/diff_cont/cmd_vel:=/robot_2/cmd_vel "
            # odom: diff_cont's real topic is /robot_2/diff_cont/odom (matches
            # its actual namespaced node name, not the un-namespaced /diff_cont)
            "-r /robot_2/diff_cont/odom:=/robot_2/odom"
            # NOTE: no /tf or /tf_static remap here on purpose. tf2 broadcasters
            # (robot_state_publisher AND diff_cont) always publish to the global
            # /tf and /tf_static topics regardless of node namespace, so both
            # need to land in the same place. Remapping just diff_cont's tf
            # elsewhere splits the tree and breaks anything that reads the
            # global /tf (RViz, slam_toolbox, view_frames).
        ],
    )

    joint_broad_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "joint_broad",
            "-c", "/robot_2/controller_manager",
            "--controller-ros-args",
            "-r /joint_states:=/robot_2/joint_states"
        ],
    )

    # Only start the controller spawners once the robot has actually been
    # spawned into Gazebo -- gz_ros2_control's controller_manager doesn't
    # exist until the entity (and its plugins) load, so launching the
    # spawners in parallel with spawn_robot risks a race where they can't
    # find /robot_2/controller_manager yet.
    delayed_controller_spawners = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=spawn_robot,
            on_exit=[diff_drive_spawner, joint_broad_spawner],
        )
    )

    # -----------------------------
    # SLAM Toolbox Instance
    # -----------------------------
    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [
                os.path.join(
                    get_package_share_directory("robot_2_bringup"),
                    "launch",
                    "slam.launch.py",
                )
            ]
        ),
    )

    return LaunchDescription(
        [
            # Remove GTK_PATH inherited from Snap-installed VS Code.
            # Without this, Gazebo GUI may load Snap's incompatible libpthread.
            UnsetEnvironmentVariable("GTK_PATH"),

            DeclareLaunchArgument(
                'use_ros2_control',
                default_value='true',
                description='Use ros2_control if true'
            ),
            world_arg,
            spawn_x_arg,
            spawn_y_arg,
            spawn_z_arg,
            spawn_yaw_arg,
            gz_resources,
            robot_state_publisher,
            gazebo,
            spawn_robot,
            ros_gz_bridge,
            delayed_controller_spawners,
            slam,
        ]
    )

    #-------LAUNCH COMMANDS--------
    # ros2 launch robot_2_bringup launch_sim.launch.py

    #-----TELEOP COMMAND------
    #ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p stamped:=true -r /cmd_vel:=/robot_2/cmd_vel

    #------ RVIZ COMMAND------
    #rviz2 -d /home/dickson-kabiru/internship2026/robot_software/robot_2_ws/src/robot_2_bringup/rviz/mapping_rviz.rviz

    #-----MAP SAVING COMMAND------
    #ros2 run nav2_map_server map_saver_cli -f ~/my_map --ros-args -p save_map_timeout:=5.0

    # "gz_args": ["-r -v4 ", world],
    #ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p stamped:=true -p use_sim_time:=true -r /cmd_vel:=/robot_2/cmd_vel