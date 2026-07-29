import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, AppendEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():

    # Include the robot_state_publisher launch file using its correct standard file name
    dk = IncludeLaunchDescription(
                PythonLaunchDescriptionSource([os.path.join(
                    get_package_share_directory('robot_2_bringup'), 'launch', 'robot_state_publisher.launch.py'
                )]), launch_arguments={'use_sim_time': 'true'}.items()
    )
    
    world = LaunchConfiguration('world')

    # Explicitly build the full path to your world file inside the description package
    default_world_path = os.path.join(
        get_package_share_directory('robot_2_description'),
        'worlds',
        'competition.sdf'
    )

    world_arg = DeclareLaunchArgument(
        'world',
        default_value=default_world_path,
        description='Full path to world file to load'
    )
        
    # Tell Gazebo exactly where to find your custom gamefield model!
    set_env_vars_resources = AppendEnvironmentVariable(
            'GZ_SIM_RESOURCE_PATH',
            os.path.join(get_package_share_directory('robot_2_description'), 'worlds', 'models')
    )

    # Include the Gazebo launch file, provided by the ros_gz_sim package
    gazebo = IncludeLaunchDescription(
                PythonLaunchDescriptionSource([os.path.join(
                    get_package_share_directory('ros_gz_sim'), 'launch', 'gz_sim.launch.py')]),
                    launch_arguments={'gz_args': ['-r -v4 ', world], 'on_exit_shutdown': 'true'}.items()
    )

    # Run the spawner node under the mandatory robot_2 namespace
    spawn_entity = Node(package='ros_gz_sim', executable='create',
                        namespace='robot_2',
                        arguments=['-topic', 'robot_description',
                                   '-name', 'robot_2',
                                   '-z', '0.5'],
                        output='screen')
              
    # Launch the ROS-gazebo bridge for namespaced topics
    bridge_params = os.path.join(get_package_share_directory('robot_2_bringup'), 'config', 'gz_bridge.yaml')
    ros_gz_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        namespace='robot_2',
        arguments=[
            '--ros-args',
            '-p',
            f'config_file:={bridge_params}',
        ]
    )

    return LaunchDescription([
        dk,
        world_arg,
        set_env_vars_resources,
        gazebo,
        spawn_entity,
        ros_gz_bridge,
    ])
