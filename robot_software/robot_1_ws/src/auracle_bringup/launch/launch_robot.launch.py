import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription, TimerAction, DeclareLaunchArgument,
    GroupAction, RegisterEventHandler,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch.event_handlers import OnProcessStart, OnProcessExit

from launch_ros.actions import Node, PushRosNamespace
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():

    bringup_pkg = get_package_share_directory('auracle_bringup')
    description_pkg = get_package_share_directory('auracle_description')

    # Single source of truth for sim time. This is the REAL ROBOT launch file, so it
    # defaults to false. Every node below reads this same value instead of hardcoding
    # its own True/False, so they can't drift out of sync with each other again.
    use_sim_time = LaunchConfiguration('use_sim_time')
    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Use simulation (Gazebo) clock if true'
    )

    namespace = LaunchConfiguration('namespace')
    declare_namespace = DeclareLaunchArgument(
        'namespace',
        default_value='',
        description='Top-level namespace applied to every node/topic below'
    )

    rsp = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'rsp.launch.py'
        )]),
        launch_arguments={'use_sim_time': use_sim_time, 'namespace': namespace}.items()
    )

    joystick = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'joystick.launch.py'
        )]),
        launch_arguments={'use_sim_time': use_sim_time, 'namespace': namespace}.items()
    )

    # Real hardware only - launch_sim.launch.py gets its /scan from the
    # Gazebo lidar plugin + ros_gz_bridge instead. Without this, running
    # launch_robot.launch.py on the actual robot gave SLAM/Nav2 no scan data
    # at all - the lidar driver was never started anywhere.
    lidar_port = LaunchConfiguration('lidar_port')
    declare_lidar_port = DeclareLaunchArgument(
        'lidar_port',
        default_value='/dev/rplidar',
        description=("Serial device for the RPLidar. Prefer a stable udev symlink "
                     "(e.g. create /dev/rplidar via udev rule) over /dev/ttyUSBx, "
                     "which can renumber across reboots/replugs.")
    )
    rplidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'rplidar.launch.py'
        )]),
        launch_arguments={'namespace': namespace, 'serial_port': lidar_port}.items()
    )

    twist_mux_params = os.path.join(bringup_pkg, 'config', 'twist_mux.yaml')
    twist_mux = Node(
        package="twist_mux",
        executable="twist_mux",
        parameters=[twist_mux_params, {'use_sim_time': use_sim_time}],
        # Relative names: under a namespace these resolve to
        # <namespace>/cmd_vel_out etc; with namespace:='' behavior is
        # unchanged from before.
        remappings=[('cmd_vel_out', 'cmd_vel_unstamped')]
    )

    twist_stamper = Node(
        package='twist_stamper',
        executable='twist_stamper',
        parameters=[{'use_sim_time': use_sim_time}],
        remappings=[('cmd_vel_in', 'cmd_vel_unstamped'),
                    ('cmd_vel_out', 'diff_cont/cmd_vel')]
    )

    # Built directly from the same xacro used by rsp.launch.py instead of the old
    # `ros2 param get --hide-type /robot_state_publisher robot_description` hack.
    # That hack raced robot_state_publisher's startup (it could run before RSP had
    # published anything, and it hardcoded the UNNAMESPACED node name /robot_state_publisher,
    # which breaks the moment namespace is anything other than ''). Generating the
    # description directly here has neither problem.
    xacro_file = os.path.join(description_pkg, 'urdf', 'robot.urdf.xacro')
    robot_description = ParameterValue(
        Command(['xacro ', xacro_file, ' sim_mode:=', use_sim_time]),
        value_type=str
    )

    controller_params_file = os.path.join(bringup_pkg, 'config', 'my_controllers.yaml')

    # NOTE on namespace: controller_manager and the three spawners below are
    # all started indirectly (TimerAction / RegisterEventHandler), and
    # actions started that way do NOT inherit a PushRosNamespace from an
    # enclosing GroupAction - the push is already popped by the time the
    # timer/event fires (confirmed upstream: ros2/launch#743). So each one
    # gets an explicit namespace= here instead of relying on the
    # GroupAction it's still listed in below (kept for the nodes that DO
    # start synchronously and so still benefit from the push).
    controller_manager = Node(
        package="controller_manager",
        executable="ros2_control_node",
        namespace=namespace,
        parameters=[{'robot_description': robot_description, 'use_sim_time': use_sim_time},
                    controller_params_file],
    )

    # Real hardware: give ros2_control_node a bit longer than sim (5s vs 3s)
    # to open the Arduino serial link and complete its handshake before
    # anything tries to talk to it.
    delayed_controller_manager = TimerAction(period=5.0, actions=[controller_manager])

    # NOTE on ordering: all three spawners used to fire simultaneously on
    # controller_manager's start (each via its own OnProcessStart handler).
    # That's a real race - they all hit controller_manager's internal
    # switch-controller lock at once, and whichever loses the race can blow
    # past its default 5s activation timeout while the others are still
    # queued waiting for the lock (this is exactly what the logs showed:
    # imu_broadcaster retrying the lock for 20+s while diff_cont's own
    # switch attempt gave up after 5s and died). Chaining them - each one
    # starts only after the previous one's spawner process exits - removes
    # the contention entirely. diff_cont goes first since it's the one that
    # actually needs to move the robot; --controller-manager-timeout is
    # bumped well past the 5s default too, since a Pi under load can be
    # meaningfully slower than a dev machine.
    SPAWNER_TIMEOUT = "30"

    diff_drive_spawner = Node(
        package="controller_manager",
        executable="spawner",
        namespace=namespace,
        arguments=["diff_cont", "--controller-manager-timeout", SPAWNER_TIMEOUT,
         "--switch-timeout", SPAWNER_TIMEOUT],
    )

    delayed_diff_drive_spawner = RegisterEventHandler(
        event_handler=OnProcessStart(
            target_action=controller_manager,
            on_start=[diff_drive_spawner],
        )
    )

    # Remapped off the default 'joint_states' - see the joint_state_publisher
    # node below for why. --controller <name> + --controller-ros-args is the
    # documented way to pass a remap to one specific spawned controller
    # (https://control.ros.org/jazzy/doc/ros2_control/controller_manager/doc/userdoc.html).
    # NOTE: unlike the other spawners, this one uses the newer
    # `--controller <name>` flag form (required for --controller-ros-args to
    # attach to the right controller) instead of the plain positional name.
    joint_broad_spawner = Node(
        package="controller_manager",
        executable="spawner",
        namespace=namespace,
        arguments=["--controller", "joint_broad",
                   "--controller-manager-timeout", SPAWNER_TIMEOUT,
                   "--switch-timeout", SPAWNER_TIMEOUT,
                   "--controller-ros-args", "--ros-args --remap joint_states:=joint_states_hw"],
    )

    delayed_joint_broad_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=diff_drive_spawner,
            on_exit=[joint_broad_spawner],
        )
    )

    imu_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        namespace=namespace,
        arguments=["imu_broadcaster", "--controller-manager-timeout", SPAWNER_TIMEOUT,
         "--switch-timeout", SPAWNER_TIMEOUT],
    )

    delayed_imu_broadcaster_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=joint_broad_spawner,
            on_exit=[imu_broadcaster_spawner],
        )
    )

    # Fills in the 4 unactuated wheel joints (left/right_wheel_one/three -
    # only left/right_wheel_two_joint have a real ros2_control hardware
    # interface/encoder) so robot_state_publisher has *something* to compute
    # their TF from instead of never receiving them at all (they showed as
    # "No transform ... to [map]" in RViz). use_mimic_tags makes it compute
    # each one from its same-side driven wheel's real position (via the
    # <mimic> tags added in robot_core.xacro) rather than leaving them
    # frozen at zero. source_list pulls the 2 real driven-wheel values from
    # joint_broad's remapped 'joint_states_hw' above and passes them through
    # unchanged; this node becomes the sole publisher of the final
    # 'joint_states' that robot_state_publisher actually subscribes to.
    # NOTE: less thoroughly verified than the rest of this pass - no ROS
    # install available to test end to end. If it misbehaves, dropping this
    # node and joint_broad's remap above reverts cleanly to the prior
    # (2-wheel-only, but definitely working) behavior.
    joint_state_publisher = Node(
        package='joint_state_publisher',
        executable='joint_state_publisher',
        parameters=[{
            'source_list': ['joint_states_hw'],
            'use_mimic_tags': True,
            'rate': 30,
            'use_sim_time': use_sim_time,
        }],
    )

    # Fuses /diff_cont/odom (wheel encoders) with /imu_broadcaster/imu (MPU6050
    # gyro) and publishes the odom->base_link transform that slam_toolbox
    # actually uses. diff_cont.enable_odom_tf is false in my_controllers.yaml
    # so this is the ONLY thing publishing that transform now.
    ekf_params_file = os.path.join(bringup_pkg, 'config', 'ekf.yaml')
    ekf_localization = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_params_file, {'use_sim_time': use_sim_time}],
    )

    # Everything except rsp (which handles its own namespacing/frame_prefix
    # internally) is namespaced here so relative topic names above resolve
    # under <namespace>/... . With namespace:='' (default) this is a no-op
    # and every topic name is identical to before.
    namespaced_nodes = GroupAction([
        PushRosNamespace(namespace),
        twist_mux,
        twist_stamper,
        delayed_controller_manager,
        delayed_diff_drive_spawner,
        delayed_joint_broad_spawner,
        delayed_imu_broadcaster_spawner,
        joint_state_publisher,
        ekf_localization,
    ])

    return LaunchDescription([
        declare_use_sim_time,
        declare_namespace,
        declare_lidar_port,
        rsp,
        joystick,
        rplidar,
        namespaced_nodes,
    ])