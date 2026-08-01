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

    package_name='robot_2_bringup'

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
        "competition.sdf",
    )

    world_arg = DeclareLaunchArgument(
        "world",
        default_value=default_world_path,
        description="World file",
    )

    # -----------------------------
    # Gazebo resource path
    # -----------------------------
    gz_resources = AppendEnvironmentVariable(
        "GZ_SIM_RESOURCE_PATH",
        os.path.join(
            get_package_share_directory("robot_2_description"),
            "worlds",
            "models",
        ),
    )

    # 1. Define the path to your parameters file (Place this above the gazebo definition)
    gazebo_params_file = os.path.join(
        get_package_share_directory(package_name), 'config', 'gazebo_params.yaml'
    )

    # -----------------------------
    # Gazebo
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
            "extra_gazebo_args": [f"--ros-args --params-file {gazebo_params_file}"]
        }.items(),
    )

    # -----------------------------
    # Spawn Robot
    # -----------------------------
    spawn_robot = Node(
        package="ros_gz_sim",
        executable="create",
        namespace="robot_2",
       arguments=['-topic', 'robot_description',
                                   '-name', 'my_robot',
                                   '-x', '-1.03',    # Move 1 meter on X axis away from center walls
                                   '-y', '2.63',    # Move 1 meter on Y axis away from center walls
                                   '-z', '0.10'],
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

    # REMAPPING CHANNELS EXPLICITLY TO MEET REVIEWER SPECIFICATIONS
    diff_drive_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "diff_cont",
            "--controller-ros-args",
            "-r /diff_cont/cmd_vel:=/robot_2/cmd_vel "
            "-r /diff_cont/odom:=/robot_2/odom "
            "-r /tf:=/robot_2/tf "
            "-r /tf_static:=/robot_2/tf_static"
        ],
    )

    joint_broad_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "joint_broad",
            "--controller-ros-args",
            "-r /joint_states:=/robot_2/joint_states"
        ],
    )


    # -----------------------------
    # SLAM Toolbox
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

    # FIX: Group everything into a single list passed into LaunchDescription
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
