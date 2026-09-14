import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():

    # -----------------------------------------------------------------
    # Autonomous navigation (AMCL localization + Nav2) for robot_2,
    # inspired by kitana-linux's src/auracle/launch/localization_launch.py
    # + navigation_launch.py (https://github.com/AmyKibara/kitana-linux).
    #
    # Rather than vendoring copies of those two files the way kitana-linux
    # does, this wraps nav2_bringup's own bringup_launch.py (the same code
    # they were copied from) directly, so robot_2 stays on whatever Nav2
    # version is installed instead of a frozen fork of it. The only robot-
    # specific piece is config/nav2_params.yaml.
    #
    # This launch file is for NAVIGATING against a map that's already been
    # built and saved. To build/update that map first, use
    # slam.launch.py (slam_toolbox), then save the map, e.g.:
    #   ros2 run nav2_map_server map_saver_cli -f <path>/my_map
    #
    # -------LAUNCH COMMAND--------
    # ros2 launch robot_2_bringup navigation.launch.py
    # (add map:=/path/to/your_map.yaml to use a different saved map)
    #
    # -----SEND A GOAL (after AMCL has converged on a 2D Pose Estimate)------
    # Use RViz's "Nav2 Goal" tool, namespaced /robot_2, or the
    # /robot_2/navigate_to_pose action directly.
    # -----------------------------------------------------------------

    bringup_dir = get_package_share_directory("nav2_bringup")
    robot_2_bringup_dir = get_package_share_directory("robot_2_bringup")

    namespace = LaunchConfiguration("namespace")
    use_sim_time = LaunchConfiguration("use_sim_time")
    map_yaml_file = LaunchConfiguration("map")
    params_file = LaunchConfiguration("params_file")
    autostart = LaunchConfiguration("autostart")

    declare_namespace_cmd = DeclareLaunchArgument(
        "namespace",
        default_value="robot_2",
        description="Top-level namespace robot_2's nodes/topics already run under",
    )

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        "use_sim_time",
        default_value="false",
        description="Use simulation (Gazebo) clock if true",
    )

    declare_map_yaml_cmd = DeclareLaunchArgument(
        "map",
        default_value=os.path.join(
            robot_2_bringup_dir, "rviz", "maps", "good_map.yaml"
        ),
        description="Full path to the map yaml file to load",
    )

    declare_params_file_cmd = DeclareLaunchArgument(
        "params_file",
        default_value=os.path.join(
            robot_2_bringup_dir, "config", "nav2_params.yaml"
        ),
        description="Full path to the Nav2 parameters file to use",
    )

    declare_autostart_cmd = DeclareLaunchArgument(
        "autostart",
        default_value="true",
        description="Automatically bring up the Nav2 lifecycle nodes",
    )

    nav2_bringup_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(bringup_dir, "launch", "bringup_launch.py")
        ),
        launch_arguments={
            "namespace": namespace,
            "use_namespace": "true",
            # We already have a slam.launch.py (slam_toolbox) for mapping;
            # this file is for localizing + navigating against a saved map.
            "slam": "False",
            "map": map_yaml_file,
            "use_sim_time": use_sim_time,
            "params_file": params_file,
            "autostart": autostart,
        }.items(),
    )

    return LaunchDescription(
        [
            declare_namespace_cmd,
            declare_use_sim_time_cmd,
            declare_map_yaml_cmd,
            declare_params_file_cmd,
            declare_autostart_cmd,
            nav2_bringup_cmd,
        ]
    )