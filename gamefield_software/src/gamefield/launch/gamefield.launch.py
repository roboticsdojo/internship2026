import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, AppendEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():

    pkg_gamefield = get_package_share_directory('gamefield')
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')

    set_model_path = AppendEnvironmentVariable(
        'GZ_SIM_RESOURCE_PATH',
        os.path.join(pkg_gamefield, 'models')
    )

    world_path = os.path.join(pkg_gamefield, 'worlds', 'gamefield.world')

    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')]),
        launch_arguments={'gz_args': f'-r {world_path}'}.items()
    )

    return LaunchDescription([
        set_model_path,
        gazebo
    ])