import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription, SetEnvironmentVariable, RegisterEventHandler,
    DeclareLaunchArgument, GroupAction,
)
from launch.event_handlers import OnProcessStart, OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node, PushRosNamespace


def generate_launch_description():

    bringup_pkg = get_package_share_directory('auracle_bringup')
    description_pkg = get_package_share_directory('auracle_description')

    namespace = LaunchConfiguration('namespace')
    declare_namespace = DeclareLaunchArgument(
        'namespace', default_value='',
        description='Top-level namespace applied to every node/topic below'
    )

    world_path = os.path.join(description_pkg, 'worlds', 'gamefield.world')
    models_path = os.path.join(description_pkg, 'models')

    set_gz_resource_path = SetEnvironmentVariable(
        name='GZ_SIM_RESOURCE_PATH',
        value=models_path
    )

    rsp = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'rsp.launch.py'
        )]), launch_arguments={'use_sim_time': 'true', 'namespace': namespace}.items()
    )

    joystick = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            bringup_pkg, 'launch', 'joystick.launch.py'
        )]), launch_arguments={'use_sim_time': 'true', 'namespace': namespace}.items()
    )

    twist_mux_params = os.path.join(bringup_pkg, 'config', 'twist_mux.yaml')
    twist_mux = Node(
        package="twist_mux",
        executable="twist_mux",
        parameters=[twist_mux_params, {'use_sim_time': True}],
        remappings=[('cmd_vel_out', 'cmd_vel_unstamped')]
    )

    twist_stamper = Node(
        package='twist_stamper',
        executable='twist_stamper',
        parameters=[{'use_sim_time': True}],
        remappings=[('cmd_vel_in', 'cmd_vel_unstamped'),
                    ('cmd_vel_out', 'diff_cont/cmd_vel')]
    )

    # Fuses diff_cont's wheel odometry with the IMU and publishes the
    # odom->base_link TF. diff_cont has enable_odom_tf:false specifically so
    # this node is the ONLY thing broadcasting that transform.
    ekf_params = os.path.join(bringup_pkg, 'config', 'ekf.yaml')
    robot_localization_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[ekf_params, {'use_sim_time': True}]
    )

    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            get_package_share_directory('ros_gz_sim'), 'launch', 'gz_sim.launch.py')]),
        launch_arguments={'gz_args': ['-r ', world_path]}.items()
    )

    # entity name/topic are namespaced explicitly (rather than via
    # PushRosNamespace, which 'ros_gz_sim create' doesn't consistently
    # honor for its -topic argument) so multiple robots can be spawned from
    # 'robot_description' in each one's own namespace without colliding.
    spawn_entity = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=[
            '-topic', 'robot_description',
            '-name', 'my_bot',
            '-x', '-2.6',
            '-y', '-1.3',
            '-z', '0.15',
            '-R', '0.0',
            '-P', '0.0',
            '-Y', '1.57'
        ],
        namespace=namespace,
        output='screen'
    )

    gz_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock',
            'scan@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan',
            'camera/image@sensor_msgs/msg/Image[gz.msgs.Image',
            'camera/camera_info@sensor_msgs/msg/CameraInfo[gz.msgs.CameraInfo',
        ],
        namespace=namespace,
        output='screen'
    )

    # Explicit namespace= on each: actions started via RegisterEventHandler
    # do NOT inherit a PushRosNamespace from an enclosing GroupAction - the
    # push is already popped by the time the event fires (confirmed
    # upstream: ros2/launch#743). Gazebo's own controller_manager is spawned
    # as part of the ros2_control plugin loaded via ros2_control.xacro, and
    # inherits its namespace from the spawned entity's namespace (set on
    # spawn_entity below), so it lines up with these spawners automatically.
    # NOTE on ordering: these used to all fire simultaneously on
    # spawn_entity's start (each via its own OnProcessStart handler). That's
    # a real race - they all hit controller_manager's internal
    # switch-controller lock at once, and whichever loses can blow past its
    # default 5s activation timeout while the others are still queued
    # waiting for the lock (this is exactly what the sim logs showed:
    # imu_broadcaster retrying the lock for 20+s while diff_cont's own
    # switch attempt gave up after 5s and died). Chaining them - each one
    # starts only after the previous spawner process exits - removes the
    # contention entirely. --controller-manager-timeout is also bumped well
    # past the 5s default for slower machines.
    SPAWNER_TIMEOUT = "30"

    diff_drive_spawner = Node(
        package="controller_manager",
        executable="spawner",
        namespace=namespace,
        arguments=["diff_cont", "--controller-manager-timeout", SPAWNER_TIMEOUT,
         "--switch-timeout", SPAWNER_TIMEOUT],
    )

    # Remapped off the default 'joint_states' - see joint_state_publisher
    # below. New `--controller <name>` flag form is required for
    # --controller-ros-args to attach to this specific controller.
    joint_broad_spawner = Node(
        package="controller_manager",
        executable="spawner",
        namespace=namespace,
        arguments=["--controller", "joint_broad",
                   "--controller-manager-timeout", SPAWNER_TIMEOUT,
                   "--switch-timeout", SPAWNER_TIMEOUT,
                   "--controller-ros-args", "--ros-args --remap joint_states:=joint_states_hw"],
    )

    imu_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        namespace=namespace,
        arguments=["imu_broadcaster", "--controller-manager-timeout", SPAWNER_TIMEOUT,
         "--switch-timeout", SPAWNER_TIMEOUT],
    )

    delayed_diff_drive_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=spawn_entity,
            on_exit=[diff_drive_spawner],
        )
    )

    delayed_joint_broad_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=diff_drive_spawner,
            on_exit=[joint_broad_spawner],
        )
    )

    delayed_imu_broadcaster_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=joint_broad_spawner,
            on_exit=[imu_broadcaster_spawner],
        )
    )

    # Fills in the 4 unactuated wheel joints - see the matching comment in
    # launch_robot.launch.py for the full explanation. Same
    # less-thoroughly-verified caveat applies here too.
    joint_state_publisher = Node(
        package='joint_state_publisher',
        executable='joint_state_publisher',
        parameters=[{
            'source_list': ['joint_states_hw'],
            'use_mimic_tags': True,
            'rate': 30,
            'use_sim_time': True,
        }],
    )

    namespaced_nodes = GroupAction([
        PushRosNamespace(namespace),
        twist_mux,
        twist_stamper,
        robot_localization_node,
        delayed_diff_drive_spawner,
        delayed_joint_broad_spawner,
        delayed_imu_broadcaster_spawner,
        joint_state_publisher,
    ])

    return LaunchDescription([
        declare_namespace,
        set_gz_resource_path,
        rsp,
        joystick,
        gazebo,
        gz_bridge,
        spawn_entity,
        namespaced_nodes,
    ])