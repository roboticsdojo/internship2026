import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():

    use_sim_time = LaunchConfiguration('use_sim_time')
    namespace = LaunchConfiguration('namespace')
    joy_params = os.path.join(
        get_package_share_directory('auracle_bringup'), 'config', 'joystick.yaml'
    )

    joy_node = Node(
        package='joy',
        executable='joy_node',
        name='joy_node',
        namespace=namespace,
        parameters=[joy_params, {'use_sim_time': use_sim_time}],
    )

    teleop_node = Node(
        package='teleop_twist_joy',
        executable='teleop_node',
        name='teleop_twist_joy_node',
        namespace=namespace,
        parameters=[joy_params, {'use_sim_time': use_sim_time}],
        # twist_mux.yaml expects the joystick's twist on this relative
        # topic; feeds into twist_mux -> cmd_vel_unstamped -> twist_stamper.
        remappings=[('cmd_vel', 'cmd_vel_joy')],
    )

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('namespace', default_value=''),
        joy_node,
        teleop_node,
    ])
