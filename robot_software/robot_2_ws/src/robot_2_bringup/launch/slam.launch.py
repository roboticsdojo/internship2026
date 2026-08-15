import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource


def generate_launch_description():

    slam_params = os.path.join(
        get_package_share_directory("robot_2_bringup"),
        "config",
        "slam_toolbox.yaml",
    )

    slam_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory("slam_toolbox"),
                "launch",
                "online_async_launch.py",
            )
        ),
        launch_arguments={
            "use_sim_time": "true",
            # NOTE: online_async_launch.py declares this argument as
            # "slam_params_file", not "params_file". A key that doesn't
            # match is silently dropped by launch (no error), and
            # slam_toolbox falls back to its own bundled default params
            # (including scan_topic: /scan) instead of this file.
            "slam_params_file": slam_params,
        }.items(),
    )

    return LaunchDescription([
        slam_launch
    ])