import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription, TimerAction, DeclareLaunchArgument,
    GroupAction, RegisterEventHandler,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration, PythonExpression
from launch.conditions import IfCondition, UnlessCondition
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

    arduino_port = LaunchConfiguration('arduino_port')
    declare_arduino_port = DeclareLaunchArgument(
        'arduino_port',
        default_value='/dev/arduino',
        description=("Serial device for the Arduino Nano (FTDI 0403:6001). /dev/arduino "
                     "comes from config/99-auracle.rules; otherwise use /dev/ttyUSB0.")
    )

    use_joy_node = LaunchConfiguration('use_joy_node')
    declare_use_joy_node = DeclareLaunchArgument(
        'use_joy_node',
        default_value='true',
        description=("true = read a controller attached to this machine. "
                     "false = /joy comes from elsewhere (e.g. the Windows->WSL UDP "
                     "bridge), so this machine runs only the drive node.")
    )

    rsp = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'rsp.launch.py'
        )]),
        launch_arguments={'use_sim_time': use_sim_time, 'namespace': namespace,
                          'arduino_port': arduino_port}.items()
    )

    joystick = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'joystick.launch.py'
        )]),
        launch_arguments={'use_sim_time': use_sim_time, 'namespace': namespace,
                          'use_joy_node': use_joy_node}.items()
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
    use_lidar = LaunchConfiguration('use_lidar')
    declare_use_lidar = DeclareLaunchArgument(
        'use_lidar', default_value='true',
        description='false = skip the RPLidar (teleop only, no SLAM/Nav2)')
    rplidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'rplidar.launch.py'
        )]),
        launch_arguments={'namespace': namespace, 'serial_port': lidar_port}.items(),
        condition=IfCondition(use_lidar),
    )

    # scan -> scan_filtered with the robot's own outline removed (config/scan_filter.yaml).
    # SLAM, AMCL and the Nav2 costmaps all read scan_filtered.
    scan_filter = Node(
        package='laser_filters',
        executable='scan_to_scan_filter_chain',
        output='screen',
        parameters=[os.path.join(bringup_pkg, 'config', 'scan_filter.yaml')],
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
        condition=IfCondition(use_lidar),
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
        Command(['xacro ', xacro_file, ' sim_mode:=', use_sim_time,
                 ' arduino_port:=', arduino_port]),
        value_type=str
    )

    controller_params_file = os.path.join(bringup_pkg, 'config', 'my_controllers.yaml')
    # Real-robot-only overrides (velocity/accel limits, calibrated geometry).
    # Loaded after the shared file so sim keeps its own values untouched.
    controller_real_params_file = os.path.join(bringup_pkg, 'config', 'my_controllers_real.yaml')

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
                    controller_params_file, controller_real_params_file],
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

    # Lidar odometry (rf2o) instead of wheel distance. On grass the wheels over-report travel by
    # 50-85 % (slip), which smears the SLAM map. With this on, forward speed in the EKF comes from
    # the scan matcher, yaw rate from the IMU gyro, and wheel odometry is not fused
    # (config/ekf_lidar_odom.yaml). Defaults to on when rf2o_laser_odometry is installed.
    try:
        get_package_share_directory('rf2o_laser_odometry')
        rf2o_installed = True
    except Exception:
        rf2o_installed = False
    use_lidar_odom = LaunchConfiguration('use_lidar_odom')
    declare_use_lidar_odom = DeclareLaunchArgument(
        'use_lidar_odom', default_value='true' if rf2o_installed else 'false',
        description=('true = fuse rf2o lidar odometry instead of wheel distance (needs '
                     'rf2o_laser_odometry built in the workspace). false = old wheel-vx EKF.'))
    lidar_odom_on = IfCondition(use_lidar_odom)

    rf2o = Node(
        package='rf2o_laser_odometry',
        executable='rf2o_laser_odometry_node',
        name='rf2o_laser_odometry',
        output='log',
        parameters=[{
            'laser_scan_topic': 'scan_filtered',   # robot's own outline already removed
            'odom_topic': 'odom_rf2o_raw',
            'publish_tf': False,                   # the EKF owns odom -> base_footprint
            'base_frame_id': 'base_footprint',
            'odom_frame_id': 'odom',
            'init_pose_from_topic': '',
            'freq': 8.0,   # lidar is ~7.5 Hz; 20 made it warn "Waiting for laser_scans" every tick
            'use_sim_time': use_sim_time,
        }],
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
        condition=IfCondition(PythonExpression(
            ["'", use_lidar_odom, "' == 'true' and '", LaunchConfiguration('use_lidar'), "' == 'true'"])),
    )
    odom_cov_relay = Node(
        package='auracle_bringup',
        executable='odom_cov_relay.py',
        name='odom_cov_relay',
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
        condition=lidar_odom_on,
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
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
        condition=UnlessCondition(use_lidar_odom),
    )
    # Same node, with the lidar-odometry overlay loaded after ekf.yaml (later file wins).
    ekf_localization_lidar = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_params_file,
                    os.path.join(bringup_pkg, 'config', 'ekf_lidar_odom.yaml'),
                    {'use_sim_time': use_sim_time}],
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
        condition=lidar_odom_on,
    )

    # Everything except rsp (which handles its own namespacing) is namespaced here so relative topic names above resolve
    # under <namespace>/... . With namespace:='' (default) this is a no-op
    # and every topic name is identical to before.
    namespaced_nodes = GroupAction([
        PushRosNamespace(namespace),
        twist_mux,
        twist_stamper,
        joint_state_publisher,
        ekf_localization,
        ekf_localization_lidar,
        rf2o,
        odom_cov_relay,
        scan_filter,
    ])

    return LaunchDescription([
        declare_use_sim_time,
        declare_namespace,
        declare_lidar_port,
        declare_use_lidar,
        declare_arduino_port,
        declare_use_joy_node,
        declare_use_lidar_odom,
        rsp,
        joystick,
        rplidar,
        namespaced_nodes,
        delayed_controller_manager,
        delayed_diff_drive_spawner,
        delayed_joint_broad_spawner,
        delayed_imu_broadcaster_spawner,
    ])