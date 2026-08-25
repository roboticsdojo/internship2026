import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
import xacro


def launch_setup(context, *args, **kwargs):
    # Resolve LaunchConfigurations to plain strings now that we're
    # inside an OpaqueFunction (runs at launch time, not at
    # generate_launch_description() build time). This is required so
    # xacro actually receives the values callers pass in via
    # IncludeLaunchDescription(launch_arguments={...}) - the previous
    # version called xacro.process_file() directly in
    # generate_launch_description(), which only ever saw default
    # values, so 'use_ros2_control' passed in from launch_sim.launch.py
    # was silently ignored.
    use_sim_time = LaunchConfiguration('use_sim_time').perform(context)
    use_ros2_control = LaunchConfiguration('use_ros2_control').perform(context)
    sim_mode = LaunchConfiguration('sim_mode').perform(context)

    pkg_path = os.path.join(get_package_share_directory('robot_2_description'))
    xacro_file = os.path.join(pkg_path, 'urdf', 'robot_2.urdf.xacro')

    robot_description_config = xacro.process_file(
        xacro_file,
        mappings={
            'use_ros2_control': use_ros2_control,
            'sim_mode': sim_mode,
        },
    ).toxml()

    params = {
        'robot_description': robot_description_config,
        'use_sim_time': use_sim_time == 'true',
    }

    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        namespace='robot_2',
        name='robot_state_publisher',
        output='screen',
        parameters=[params],
    )

    return [node_robot_state_publisher]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='false',
            description='Use sim time if true'),
        DeclareLaunchArgument(
            'use_ros2_control',
            default_value='true',
            description='Use ros2_control if true'),
        DeclareLaunchArgument(
            'sim_mode',
            default_value='false',
            description=(
                'Selects Gazebo vs real-robot hardware inside '
                'ros2_control.xacro. Only meaningful when '
                'use_ros2_control is true.'
            )),
        OpaqueFunction(function=launch_setup),
    ])