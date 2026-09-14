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
    Shared Localization + Nav2 + RViz stack -- the counterpart to
    slam_nav_rviz.launch.py. Use THIS one once you already have a saved map
    and just want the robot to localize against it and navigate, instead of
    building a new map every time.

    Launched identically by sim and real robot (only use_sim_time differs),
    same as slam_nav_rviz.launch.py.
    """
    bringup_pkg = get_package_share_directory('auracle_bringup')

    use_sim_time = LaunchConfiguration('use_sim_time')
    namespace = LaunchConfiguration('namespace')
    use_rviz = LaunchConfiguration('use_rviz')
    map_yaml_file = LaunchConfiguration('map')

    declare_use_sim_time = DeclareLaunchArgument(
        'use_sim_time', default_value='false',
        description='Use simulation (Gazebo) clock if true')
    declare_namespace = DeclareLaunchArgument(
        'namespace', default_value='',
        description='Top-level namespace')
    declare_use_rviz = DeclareLaunchArgument(
        'use_rviz', default_value='true',
        description='Whether to launch RViz')
    declare_map = DeclareLaunchArgument(
        'map',
        default_value=os.path.join(bringup_pkg, 'config', 'map_1.yaml'),
        description='Full path to the saved map yaml file to localize against')

    localization_params = os.path.join(bringup_pkg, 'config', 'nav2_params.yaml')
    localization = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [os.path.join(bringup_pkg, 'launch', 'localization_neo_launch.py')]),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'namespace': namespace,
            'map': map_yaml_file,
            'params_file': localization_params,
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
        declare_map,
        localization,
        navigation,
        GroupAction([PushRosNamespace(namespace), rviz_node]),
    ])