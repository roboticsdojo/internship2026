import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.substitutions import LaunchConfiguration, Command, PythonExpression
from launch.actions import DeclareLaunchArgument, GroupAction
from launch_ros.actions import Node, PushRosNamespace
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():

    use_sim_time = LaunchConfiguration('use_sim_time')
    namespace = LaunchConfiguration('namespace')

    pkg_path = os.path.join(get_package_share_directory('auracle_description'))
    xacro_file = os.path.join(pkg_path, 'urdf', 'robot.urdf.xacro')

    robot_description_config = ParameterValue(
        Command([
            'xacro ', xacro_file,
            ' sim_mode:=', use_sim_time
        ]),
        value_type=str
    )

    # When namespaced, TF frames (base_link, odom, laser_frame, ...) need a
    # matching prefix or every namespaced robot's frames collide in one TF
    # tree. robot_state_publisher does this via frame_prefix; left empty
    # (namespace:='') this is a no-op and behavior is unchanged.
    frame_prefix = PythonExpression(["'", namespace, "/' if '", namespace, "' else ''"])

    params = {
        'robot_description': robot_description_config,
        'use_sim_time': use_sim_time,
        'frame_prefix': frame_prefix,
    }
    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[params]
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

        GroupAction([
            PushRosNamespace(namespace),
            node_robot_state_publisher,
        ]),
    ])
