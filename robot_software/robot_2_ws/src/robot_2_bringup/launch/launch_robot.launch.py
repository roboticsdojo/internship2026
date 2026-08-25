import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node


def generate_launch_description():

    # -----------------------------
    # Robot State Publisher
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
            # Selects the real-robot <ros2_control> block in
            # ros2_control.xacro (Arduino/L298 via robot_2_hardware)
            # instead of GazeboSimSystem.
            'sim_mode': 'false',
        }.items()
    )

    # -----------------------------
    # Controller Manager
    # -----------------------------
    # Unlike sim (where gz_ros2_control runs controller_manager as part
    # of the Gazebo plugin), the real robot needs its own
    # ros2_control_node process reading robot_description from the
    # topic robot_state_publisher publishes, and its own real-robot
    # controller params (use_sim_time: false, unlike my_controllers.yaml
    # which is sim-only and hardcodes use_sim_time: true throughout).
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
    # Same remap pattern as launch_sim.launch.py so teleop and any tools
    # that publish to /robot_2/cmd_vel or read /robot_2/odom work
    # identically whether the robot is real or simulated.
    diff_drive_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "diff_cont",
            "-c", "/robot_2/controller_manager",
            "--controller-ros-args",
            "-r /robot_2/diff_cont/cmd_vel:=/robot_2/cmd_vel "
            "-r /robot_2/diff_cont/odom:=/robot_2/odom"
            # NOTE: no /tf or /tf_static remap - see launch_sim.launch.py
            # for why (global tf tree must not be split).
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

    return LaunchDescription(
        [
            robot_state_publisher,
            controller_manager,
            diff_drive_spawner,
            joint_broad_spawner,
        ]
    )

    #-------LAUNCH COMMAND--------
    # ros2 launch robot_2_bringup launch_robot.launch.py

    #-----TELEOP COMMAND------
    # Same topic as sim, thanks to the cmd_vel remap above:
    # ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p stamped:=true -r /cmd_vel:=/robot_2/cmd_vel