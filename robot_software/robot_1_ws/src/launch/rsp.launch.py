import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.substitutions import LaunchConfiguration, Command
from launch.actions import DeclareLaunchArgument
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue

def generate_launch_description():

    use_sim_time = LaunchConfiguration('use_sim_time')

    pkg_path = os.path.join(get_package_share_directory('auracle'))
    xacro_file = os.path.join(pkg_path, 'description', 'robot.urdf.xacro')

    # Correctly format sim_mode as 'true'/'false' for Xacro parsing.
    # ParameterValue(..., value_type=str) is required on Jazzy: without it,
    # launch_ros tries to auto-detect the parameter type and attempts to
    # YAML-parse the raw URDF/XML text coming out of Command(), which fails
    # with "Unable to parse the value of parameter robot_description as yaml".
    # This didn't show up on Humble-era tutorials this repo is based on;
    # Jazzy's type inference is stricter.
    robot_description_config = ParameterValue(
        Command([
            'xacro ', xacro_file,
            ' sim_mode:=', use_sim_time
        ]),
        value_type=str
    )
    
    params = {'robot_description': robot_description_config, 'use_sim_time': use_sim_time}
    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[params]
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='true',
            description='Use sim time if true'),

        node_robot_state_publisher
    ])