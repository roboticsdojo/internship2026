import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    RegisterEventHandler,
    TimerAction,
)
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
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

    # Servo (offload) - ForwardCommandController on the "servo/open"
    # command interface. Listens on /robot_2/servo_controller/commands
    # (std_msgs/Float64MultiArray, data: [1.0] = open, [0.0] = closed).
    # The behavior tree publishes to this topic.
    servo_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "servo_controller",
            "-c", "/robot_2/controller_manager",
        ],
    )

    # -----------------------------
    # 2. LiDAR
    # -----------------------------
    # Intermittently fails at startup with "Can not start scan: 80008002"
    # (SDK operation timeout) and exits with code 255. Two mitigations:
    #   - respawn=True: relaunch the node automatically after it dies.
    #     By the 2nd try the motor has usually spun up and it connects.
    #   - it is started only AFTER the controllers are up (see sequencing
    #     below), so the Arduino reset, controller loading and slam_toolbox
    #     are not competing for CPU/USB power at the same instant.
    lidar_node = Node(
        package='rplidar_ros',
        executable='rplidar_composition',
        namespace='robot_2',
        output='screen',
        respawn=True,
        respawn_delay=3.0,
        parameters=[{
            'serial_port': '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0',
            'serial_baudrate': 115200,
            'frame_id': 'laser_frame',
            'angle_compensate': True,
            'scan_mode': 'Standard'
        }]
    )

    # -----------------------------
    # 3. slam_toolbox - MAPPING ONLY,
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
    # Sequencing
    # -----------------------------
    # Spawners run one after another so the controller_manager is never
    # asked to load two controllers at once:
    #   diff_cont + joint_broad (started with the controller manager)
    #     -> joint_broad exits -> servo_controller spawner starts
    #     -> servo spawner exits -> wait 2 s, start the lidar;
    #                               wait a further 6 s, start slam_toolbox.
    #
    # OnProcessExit fires whether the spawner succeeded or failed, so the
    # lidar still starts if the servo controller cannot be loaded (e.g. the
    # servo <gpio> is removed from the URDF).
    #
    # slam_toolbox is started on a timer rather than via OnProcessStart on
    # the lidar node: with respawn=True, OnProcessStart would fire again on
    # every lidar restart and try to launch a second slam_toolbox.
    # slam_toolbox's IfCondition makes this a no-op when slam:=false.
    start_servo = RegisterEventHandler(
        OnProcessExit(
            target_action=joint_broad_spawner,
            on_exit=[servo_spawner],
        )
    )

    start_lidar_and_slam = RegisterEventHandler(
        OnProcessExit(
            target_action=servo_spawner,
            on_exit=[
                TimerAction(period=2.0, actions=[lidar_node]),
                TimerAction(period=8.0, actions=[slam_toolbox]),
            ],
        )
    )

    return LaunchDescription(
        [
            declare_slam_cmd,
            robot_state_publisher,
            controller_manager,
            diff_drive_spawner,
            joint_broad_spawner,
            start_servo,
            start_lidar_and_slam,
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

    # -----SERVO TEST (after bringup)------
    #   ros2 control list_controllers -c /robot_2/controller_manager
    #   ros2 topic pub --once /robot_2/servo_controller/commands std_msgs/msg/Float64MultiArray "{data: [1.0]}"   # open  (g)
    #   ros2 topic pub --once /robot_2/servo_controller/commands std_msgs/msg/Float64MultiArray "{data: [0.0]}"   # close (h)