import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.substitutions import LaunchConfiguration, Command
from launch.actions import DeclareLaunchArgument, GroupAction
from launch_ros.actions import Node, PushRosNamespace
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():

    use_sim_time = LaunchConfiguration('use_sim_time')
    namespace = LaunchConfiguration('namespace')
    arduino_port = LaunchConfiguration('arduino_port')

    pkg_path = os.path.join(get_package_share_directory('auracle_description'))
    xacro_file = os.path.join(pkg_path, 'urdf', 'robot.urdf.xacro')

    robot_description_config = ParameterValue(
        Command([
            'xacro ', xacro_file,
            ' sim_mode:=', use_sim_time,
            ' arduino_port:=', arduino_port,
        ]),
        value_type=str
    )

    params = {
        'robot_description': robot_description_config,
        'use_sim_time': use_sim_time,
    }
    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[params],
        # Each namespace gets its own TF tree (<ns>/tf), like the Nav2 launches.
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='false',
            description='Use sim time if true'),
        DeclareLaunchArgument(
            'namespace',
            default_value='',
            description='Top-level namespace, e.g. robot1. Leave empty for a single robot.'),
        # Real robot only. In Jazzy the controller_manager takes its URDF from
        # this node's robot_description topic, so these must match launch_robot.
        DeclareLaunchArgument(
            'arduino_port',
            default_value='/dev/arduino',
            description='Serial device of the Arduino Nano (real robot only)'),

        GroupAction([
            PushRosNamespace(namespace),
            node_robot_state_publisher,
        ]),
    ])
