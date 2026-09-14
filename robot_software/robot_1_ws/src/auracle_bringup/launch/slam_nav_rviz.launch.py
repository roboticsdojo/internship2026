import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, GroupAction
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace


def generate_launch_description():
    """
    Shared SLAM + Nav2 + RViz stack. Launched identically by both
    launch_sim.launch.py (use_sim_time:=true) and launch_robot.launch.py
    (use_sim_time:=false) so sim and the real robot run the exact same
    localization/planning pipeline - only the clock source differs.
    """
    bringup_pkg = get_package_share_directory('auracle_bringup')

    use_sim_time = LaunchConfiguration('use_sim_time')
    namespace = LaunchConfiguration('namespace')
    use_rviz = LaunchConfiguration('use_rviz')

    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time', default_value='false',
        description='Use simulation (Gazebo) clock if true')
    declare_namespace = DeclareLaunchArgument(
        'namespace', default_value='',
        description='Top-level namespace')
    declare_use_rviz = DeclareLaunchArgument(
        'use_rviz', default_value='true',
        description='Whether to launch RViz')

    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [os.path.join(bringup_pkg, 'launch', 'online_async_launch.py')]),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'namespace': namespace,
        }.items()
    )

    nav_params = os.path.join(bringup_pkg, 'config', 'nav2_params.yaml')
    navigation = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [os.path.join(bringup_pkg, 'launch', 'navigation_launch.py')]),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'namespace': namespace,
            'params_file': nav_params,  
        }.items()
    )

    rviz_config = os.path.join(bringup_pkg, 'rviz', 'auracle.rviz')
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        namespace=namespace,
        arguments=['-d', rviz_config],
        parameters=[{'use_sim_time': use_sim_time}],
        output='screen',
        condition=IfCondition(use_rviz),
    )

    return LaunchDescription([
        declare_use_sim_time,
        declare_namespace,
        declare_use_rviz,
        slam,
        navigation,
        GroupAction([PushRosNamespace(namespace), rviz_node]),
    ])
