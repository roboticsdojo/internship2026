import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription,
    RegisterEventHandler,
)
from launch.event_handlers import OnProcessStart
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node


def generate_launch_description():

    # -----------------------------
    # 1. Robot State Publisher (starts first)
    # -----------------------------
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
            'use_sim_time': 'false',
            'use_ros2_control': 'true',
            'sim_mode': 'false',
        }.items()
    )

    # -----------------------------
    # Controller Manager
    # -----------------------------
    controller_manager = Node(
        package="controller_manager",
        executable="ros2_control_node",
        namespace="robot_2",
        name="controller_manager",
        parameters=[
            os.path.join(
                get_package_share_directory("robot_2_bringup"),
                "config",
                "my_controllers_real.yaml",
            )
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
            "-r /robot_2/diff_cont/cmd_vel:=/robot_2/cmd_vel "
            "-r /robot_2/diff_cont/odom:=/robot_2/odom"
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
    # 2. LiDAR (starts after controller_manager comes up)
    # -----------------------------
    lidar_node = Node(
        package='rplidar_ros',
        executable='rplidar_composition',
        namespace='robot_2',
        output='screen',
        parameters=[{
            'serial_port': '/dev/ttyUSB0',
            'serial_baudrate': 115200,
            'frame_id': 'laser_frame',
            'angle_compensate': True,
            'scan_mode': 'Standard'
        }]
    )

    # -----------------------------
    # 3. slam_toolbox (starts after lidar node comes up)
    # -----------------------------
    slam_toolbox = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [
                os.path.join(
                    get_package_share_directory("slam_toolbox"),
                    "launch",
                    "online_async_launch.py",
                )
            ]
        ),
        launch_arguments={
            'slam_params_file': os.path.join(
                get_package_share_directory("robot_2_bringup"),
                "config",
                "slam_toolbox.yaml",
            ),
            'use_sim_time': 'false',
        }.items()
    )

    # -----------------------------
    # Event-based sequencing
    # -----------------------------
    # Once controller_manager actually starts, launch the LiDAR.
    start_lidar_after_controller_manager = RegisterEventHandler(
        OnProcessStart(
            target_action=controller_manager,
            on_start=[lidar_node],
        )
    )

    # Once the LiDAR node actually starts, launch slam_toolbox.
    start_slam_after_lidar = RegisterEventHandler(
        OnProcessStart(
            target_action=lidar_node,
            on_start=[slam_toolbox],
        )
    )

    return LaunchDescription(
        [
            robot_state_publisher,
            controller_manager,
            diff_drive_spawner,
            joint_broad_spawner,
            start_lidar_after_controller_manager,
            start_slam_after_lidar,
        ]
    )

    # -------LAUNCH COMMAND--------
    # ros2 launch robot_2_bringup full_bringup.launch.py

    # -----TELEOP COMMAND------
    # ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p stamped:=true -r /cmd_vel:=/robot_2/cmd_vel