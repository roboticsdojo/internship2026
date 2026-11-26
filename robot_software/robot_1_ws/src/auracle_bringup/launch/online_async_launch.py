import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument, EmitEvent, LogInfo, RegisterEventHandler,
)
from launch.conditions import IfCondition
from launch.events import matches_action
from launch.substitutions import (
    AndSubstitution, LaunchConfiguration, NotSubstitution,
)
from launch_ros.actions import LifecycleNode, Node
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition
from nav2_common.launch import RewrittenYaml


def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time')
    namespace = LaunchConfiguration('namespace')
    params_file = LaunchConfiguration('params_file')
    autostart = LaunchConfiguration('autostart')
    use_lifecycle_manager = LaunchConfiguration('use_lifecycle_manager')

    default_params_file = os.path.join(
        get_package_share_directory('auracle_bringup'),
        'config', 'mapper_params_online_async.yaml')

    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Use simulation/Gazebo clock')
    declare_namespace = DeclareLaunchArgument(
        'namespace', default_value='',
        description='Top-level namespace (e.g. auracle on the real robot).')
    declare_params_file = DeclareLaunchArgument(
        'params_file', default_value=default_params_file,
        description='Full path to the slam_toolbox parameter file')
    declare_autostart = DeclareLaunchArgument(
        'autostart', default_value='true',
        description='Configure + activate slam_toolbox automatically')
    declare_use_lifecycle_manager = DeclareLaunchArgument(
        'use_lifecycle_manager', default_value='false',
        description=('false (default) = launch drives configure/activate itself, no bond '
                     'heartbeat, so a loaded Pi cannot get SLAM torn down and restarted. '
                     'true = old behaviour: nav2 lifecycle_manager_slam.'))

    # slam_toolbox's yaml key is a bare "slam_toolbox:", which only matches the node
    # /slam_toolbox. With namespace:=auracle the node is /auracle/slam_toolbox, so the
    # file was silently NOT applied (defaults used instead of mapper_params_online_async.yaml).
    # RewrittenYaml nests the file under the namespace, exactly like the Nav2 launches do.
    configured_params = RewrittenYaml(
        source_file=params_file,
        root_key=namespace,
        param_rewrites={},
        convert_types=True)

    slam_node = LifecycleNode(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        namespace=namespace,
        output='screen',
        parameters=[
            configured_params,
            {'use_sim_time': use_sim_time,
             'use_lifecycle_manager': use_lifecycle_manager},
        ],
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static'),
                    ('/scan', 'scan'), ('/map', 'map'),
                    ('/map_metadata', 'map_metadata')],
    )

    drive_it_ourselves = IfCondition(
        AndSubstitution(autostart, NotSubstitution(use_lifecycle_manager)))

    configure_event = EmitEvent(
        event=ChangeState(
            lifecycle_node_matcher=matches_action(slam_node),
            transition_id=Transition.TRANSITION_CONFIGURE),
        condition=drive_it_ourselves)

    activate_event = RegisterEventHandler(
        OnStateTransition(
            target_lifecycle_node=slam_node,
            start_state='configuring',
            goal_state='inactive',
            entities=[
                LogInfo(msg='[slam] slam_toolbox configured, activating.'),
                EmitEvent(event=ChangeState(
                    lifecycle_node_matcher=matches_action(slam_node),
                    transition_id=Transition.TRANSITION_ACTIVATE)),
            ]),
        condition=drive_it_ourselves)

    # Optional old path. bond_timeout 0.0 disables the heartbeat check entirely.
    lifecycle_manager = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_slam',
        namespace=namespace,
        output='screen',
        parameters=[{'autostart': True,
                     'node_names': ['slam_toolbox'],
                     'bond_timeout': 0.0,
                     'use_sim_time': use_sim_time}],
        condition=IfCondition(use_lifecycle_manager))

    return LaunchDescription([
        declare_use_sim_time,
        declare_namespace,
        declare_params_file,
        declare_autostart,
        declare_use_lifecycle_manager,
        slam_node,
        activate_event,
        configure_event,
        lifecycle_manager,
    ])
