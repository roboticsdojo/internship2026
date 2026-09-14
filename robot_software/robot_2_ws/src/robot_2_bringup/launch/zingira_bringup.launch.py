import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    RegisterEventHandler,
)
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessStart
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node


def generate_launch_description():

    # -----------------------------
    # Toggle for SLAM mapping mode.
    # -----------------------------
    # slam_toolbox (mapping) and Nav2's map_server+AMCL (navigation)
    # both publish to the global /map topic. Running both at once
    # means they fight over it - slam_toolbox republishes its own
    # partial, growing map every map_update_interval seconds and
    # stomps on map_server's static saved map. Default false here:
    # use slam:=true only while actively building/updating a map;
    # leave it false when you're about to bring up
    # nav2_bringup.launch.py for actual navigation.
    slam_arg = LaunchConfiguration('slam')
    declare_slam_cmd = DeclareLaunchArgument(
        'slam',
        default_value='false',
        description='Launch slam_toolbox in mapping mode. Set false '
                     'when using map_server+AMCL for navigation instead.',
    )

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
            'serial_port': '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0',
            'serial_baudrate': 115200,
            'frame_id': 'laser_frame',
            'angle_compensate': True,
            'scan_mode': 'Standard'
        }]
    )

    # -----------------------------
    # 3. slam_toolbox (starts after lidar node comes up) - MAPPING ONLY,
    # only included at all when slam:=true is passed.
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
        }.items(),
        condition=IfCondition(slam_arg),
    )

    # -----------------------------
    # Event-based sequencing
    # -----------------------------
    start_lidar_after_controller_manager = RegisterEventHandler(
        OnProcessStart(
            target_action=controller_manager,
            on_start=[lidar_node],
        )
    )

    # slam_toolbox's own IfCondition above means this simply no-ops
    # when slam:=false - no separate condition needed here.
    start_slam_after_lidar = RegisterEventHandler(
        OnProcessStart(
            target_action=lidar_node,
            on_start=[slam_toolbox],
        )
    )

    return LaunchDescription(
        [
            declare_slam_cmd,
            robot_state_publisher,
            controller_manager,
            diff_drive_spawner,
            joint_broad_spawner,
            start_lidar_after_controller_manager,
            start_slam_after_lidar,
        ]
    )

    # -------LAUNCH COMMANDS--------
    # Mapping mode (build/update a map):
    #   ros2 launch robot_2_bringup zingira_bringup.launch.py slam:=true
    #
    # Navigation mode (robot + lidar only, no SLAM):
    #   ros2 launch robot_2_bringup zingira_bringup.launch.py
    #   (then, in a SEPARATE terminal, once that's stable:)
    #   ros2 launch robot_2_bringup nav2_bringup.launch.py map:=<path/to/map.yaml>

    # -----TELEOP COMMAND------
    # ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p stamped:=true -r /cmd_vel:=/robot_2/cmd_vel