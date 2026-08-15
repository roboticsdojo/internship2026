import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription,
    DeclareLaunchArgument,
    AppendEnvironmentVariable,
)
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
        launch_arguments={'use_sim_time': 'true', 'use_ros2_control': use_ros2_control}.items()
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
            "gz_args": ["-r -v4 ", world],
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
                   '-z', '0.4'],
        output="screen",
    )

    joint_state_publisher = Node(
        package="joint_state_publisher",
        executable="joint_state_publisher",
        namespace="robot_2",
        parameters=[
            {"use_sim_time": True},
        ],
        output="screen",
    )

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
            DeclareLaunchArgument(
                'use_ros2_control',
                default_value='true',
                description='Use ros2_control if true'
            ),
            world_arg,
            gz_resources,
            robot_state_publisher,
            joint_state_publisher,
            gazebo,
            spawn_robot,
            ros_gz_bridge,
            diff_drive_spawner,
            joint_broad_spawner,
            slam,
        ]
    )