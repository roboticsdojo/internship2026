import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.substitutions import Command, LaunchConfiguration
from launch.actions import DeclareLaunchArgument
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue

def generate_launch_description():

    # Check if we're told to use sim time
    use_sim_time = LaunchConfiguration('use_sim_time')
    use_ros2_control = LaunchConfiguration('use_ros2_control')

    # Process the URDF file
    pkg_path = os.path.join(get_package_share_directory('auracle'))
    xacro_file = os.path.join(pkg_path, 'description', 'robot.urdf.xacro')
    
    robot_description_config = Command([
        'xacro ', xacro_file, 
        ' use_ros2_control:=', use_ros2_control, 
        ' sim_mode:=', use_sim_time
    ])
    
# Create a robot_state_publisher node
    robot_description = ParameterValue(robot_description_config, value_type=str)
    params = {'robot_description': robot_description, 'use_sim_time': use_sim_time}
    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[params]
    )

    # FIXED INDENTATION: Aligned precisely to 4 spaces
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen'
    )

    # Launch!
    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='false',
            description='Use sim time if true'),
        DeclareLaunchArgument(
            'use_ros2_control',
            default_value='true',
            description='Use ros2_control if true'),

        node_robot_state_publisher,
        rviz_node
    ])
