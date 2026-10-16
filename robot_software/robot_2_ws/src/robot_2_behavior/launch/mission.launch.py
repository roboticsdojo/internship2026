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
        # Switch to waypoints_gamefield.yaml for the real gamefield map.
        DeclareLaunchArgument(
            'waypoints',
            default_value=os.path.join(pkg, 'config', 'waypoints_instruction.yaml')),
        Node(
            package='robot_2_behavior',
            executable='mission_bt',
            output='screen',
            parameters=[
                LaunchConfiguration('waypoints'),
                {
                    'use_sim_time': ParameterValue(use_sim_time, value_type=bool),
                    'tree_file': LaunchConfiguration('tree_file'),
                    'groot_port': 1667,
                },
            ],
        ),
    ])