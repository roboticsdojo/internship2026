import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, AppendEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():

    package_name = 'auracle'

    # 1. Look up relevant packages
    pkg_auracle = get_package_share_directory(package_name)
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')

    # 2. Tell Gazebo Sim where to find your custom gamefield 3D meshes/models
    set_model_path = AppendEnvironmentVariable(
        'GZ_SIM_RESOURCE_PATH',
        os.path.join(pkg_auracle, 'models')
    )

    # 3. Include Robot State Publisher (RSP)
    rsp = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(pkg_auracle, 'launch', 'rsp.launch.py')]),
        launch_arguments={'use_sim_time': 'true', 'use_ros2_control': 'false'}.items()
    )

    # 4. Point Gazebo Sim to your custom gamefield world file
    world_path = os.path.join(pkg_auracle, 'worlds', 'gamefield.world')

    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')]),
        launch_arguments={'gz_args': f'-r {world_path}'}.items()
    )

    # 5. Spawn the auracle robot entity into Gazebo Sim
    spawn_entity = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=[
            '-topic', 'robot_description', 
            '-name', 'auracle',
            '-x', '0.0',       
            '-y', '-0.5',      
            '-z', '1.0',        
        ],
        output='screen'
    )


    # Assemble only the essential components into execution queue
    return LaunchDescription([
        set_model_path,
        rsp,
        gazebo,
        spawn_entity
    ])
