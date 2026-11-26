import os
import tempfile

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.conditions import IfCondition
from launch_ros.actions import Node


def _rviz(context, *args, **kwargs):
    """RViz with slam.rviz, topic names prefixed with the namespace.

    RViz display topics in the .rviz file are absolute (/map, /scan, ...). Under
    namespace:=auracle they must be /auracle/map etc., so the file has __NS__ placeholders
    that are filled in here and written to a temp file.
    """
    if context.perform_substitution(LaunchConfiguration('use_rviz')).lower() != 'true':
        return []
    pkg = get_package_share_directory('auracle_bringup')
    ns = context.perform_substitution(LaunchConfiguration('namespace')).strip('/')
    use_sim_time = context.perform_substitution(
        LaunchConfiguration('use_sim_time')).lower() == 'true'

    with open(os.path.join(pkg, 'rviz', 'slam.rviz')) as f:
        text = f.read().replace('__NS__', f'/{ns}' if ns else '')
    cfg = os.path.join(tempfile.gettempdir(), f'auracle_slam_{ns or "root"}.rviz')
    with open(cfg, 'w') as f:
        f.write(text)

    return [Node(
        package='rviz2', executable='rviz2', name='rviz2', namespace=ns,
        arguments=['-d', cfg],
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
        parameters=[{'use_sim_time': use_sim_time}],
        output='screen')]


def generate_launch_description():
    """SLAM + RViz only. NO Nav2, NO AMCL, NO map_server, NO saved map.

    Use this while driving around (joystick/keyboard) to build a map. Once saved, use
    localization_nav_rviz.launch.py for navigation. slam_nav_rviz.launch.py still exists
    for the rare case where you want to navigate while mapping.
    """
    bringup_pkg = get_package_share_directory('auracle_bringup')

    use_sim_time = LaunchConfiguration('use_sim_time')
    namespace = LaunchConfiguration('namespace')
    params_file = LaunchConfiguration('params_file')

    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(bringup_pkg, 'launch', 'online_async_launch.py')),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'namespace': namespace,
            'params_file': params_file,
        }.items(),
        condition=IfCondition(LaunchConfiguration('use_slam')))

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false',
                              description='true in Gazebo, false on the real robot'),
        DeclareLaunchArgument('namespace', default_value='',
                              description='auracle on the real robot, empty in sim'),
        DeclareLaunchArgument('use_rviz', default_value='true',
                              description='false on a headless Pi; run RViz on the laptop'),
        DeclareLaunchArgument('use_slam', default_value='true',
                              description='false = RViz only (laptop), with SLAM itself running on the Pi'),
        DeclareLaunchArgument(
            'params_file',
            default_value=os.path.join(bringup_pkg, 'config', 'mapper_params_online_async.yaml'),
            description='slam_toolbox parameters'),
        slam,
        OpaqueFunction(function=_rviz),
    ])
