import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    pkg = get_package_share_directory('robot_2_behavior')
    use_sim_time = LaunchConfiguration('use_sim_time')

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument(
            'tree_file', default_value=os.path.join(pkg, 'trees', 'mission.xml')),
        Node(
            package='robot_2_behavior',
            executable='mission_bt',
            output='screen',
            parameters=[{
                'use_sim_time': ParameterValue(use_sim_time, value_type=bool),
                'tree_file': LaunchConfiguration('tree_file'),
                'groot_port': 1667,
            }],
        ),
    ])

    #dickson    - 192.168.4.182
    #tailscale  - 100.78.228.85 