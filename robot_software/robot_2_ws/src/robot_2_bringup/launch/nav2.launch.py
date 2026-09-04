import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.actions import IncludeLaunchDescription


def generate_launch_description():

    nav2_params = os.path.join(
        get_package_share_directory("robot_2_bringup"),
        "config",
        "nav2_params.yaml",
    )

    default_map = os.path.join(
        get_package_share_directory("robot_2_bringup"),
        "rviz",
        "maps",
        "good_map.yaml",
    )

    declare_map = DeclareLaunchArgument(
        "map",
        default_value=default_map,
        description="Full path to the saved map yaml file to navigate against",
    )

    nav2_bringup_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory("nav2_bringup"),
                "launch",
                "bringup_launch.py",
            )
        ),
        launch_arguments={
            "use_sim_time": "true",
            "map": LaunchConfiguration("map"),
            "params_file": nav2_params,
            "namespace": "/robot_2",
            "use_namespace": "true",
        }.items(),
    )

    return LaunchDescription([
        declare_map,
        nav2_bringup_launch,
    ])
