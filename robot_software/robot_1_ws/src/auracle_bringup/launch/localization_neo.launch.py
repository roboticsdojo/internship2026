# Based on nav2_bringup's localization_launch.py (Copyright 2018 Intel Corporation,
# Apache License 2.0), modified to launch neo_localization2 instead of nav2_amcl.
#
# WHY THIS FILE EXISTS SEPARATELY FROM localization_launch.py:
# Your original localization_launch.py (AMCL-based) is left untouched so you can
# switch back to it any time by just changing which file slam_nav_rviz-style
# launch files include. This file is the neo_localization2 equivalent.
#
# WHAT THIS NODE NEEDS TO ALREADY BE RUNNING / TRUE:
#   - A saved map (.yaml + .pgm) on disk -- see README "Saving a map" section
#   - odom -> base_link tf being published (your hardware/sim interface)
#   - A laser scan on the 'scan_topic' param (default: scan)
#   - An initial pose, either via RViz "2D Pose Estimate" or a published
#     /initialpose message
#
# TODO before first run:
#   1. `colcon build` this package, then run `ros2 pkg executables neo_localization2`
#      to confirm the exact executable name. It's assumed to be
#      'neo_localization_node' below (matches the ROS1 node name per Neobotix's
#      docs), but this hasn't been independently verified against the ROS2 port.
#   2. Confirm whether the node implements the ROS2 lifecycle interface. If
#      `ros2 lifecycle get /neo_localization_node` errors out after launching it
#      manually, it is NOT lifecycle-managed -- remove it from `lifecycle_nodes`
#      below and just let it run as a plain node (see commented-out fallback).

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, SetEnvironmentVariable
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml


def generate_launch_description():
    bringup_dir = get_package_share_directory('auracle_bringup')

    namespace = LaunchConfiguration('namespace')
    map_yaml_file = LaunchConfiguration('map')
    use_sim_time = LaunchConfiguration('use_sim_time')
    autostart = LaunchConfiguration('autostart')
    params_file = LaunchConfiguration('params_file')

    # If neo_localization_node turns out NOT to be lifecycle-managed, delete it
    # from this list (map_server will still be lifecycle-managed fine on its own).
    lifecycle_nodes = ['map_server', 'neo_localization_node']

    remappings = [('/tf', 'tf'),
                  ('/tf_static', 'tf_static')]

    param_substitutions = {
        'use_sim_time': use_sim_time,
        'yaml_filename': map_yaml_file}

    configured_params = RewrittenYaml(
        source_file=params_file,
        root_key=namespace,
        param_rewrites=param_substitutions,
        convert_types=True)

    return LaunchDescription([
        SetEnvironmentVariable('RCUTILS_LOGGING_BUFFERED_STREAM', '1'),

        DeclareLaunchArgument(
            'namespace', default_value='',
            description='Top-level namespace'),

        DeclareLaunchArgument(
            'map',
            default_value=os.path.join(bringup_dir, 'config', 'map_1.yaml'),
            description='Full path to map yaml file to load'),

        DeclareLaunchArgument(
            'use_sim_time', default_value='false',
            description='Use simulation (Gazebo) clock if true'),

        DeclareLaunchArgument(
            'autostart', default_value='true',
            description='Automatically startup the localization stack'),

        DeclareLaunchArgument(
            'params_file',
            default_value=os.path.join(bringup_dir, 'config', 'nav2_params.yaml'),
            description=(
                'Full path to the ROS2 parameters file to use. Must contain a '
                '"neo_localization_node:" block -- see config/neo_localization_params.yaml '
                'for a starter, either merge it into nav2_params.yaml or point this arg '
                'at it directly.')),

        Node(
            package='nav2_map_server',
            executable='map_server',
            name='map_server',
            namespace=namespace,
            output='screen',
            parameters=[configured_params],
            remappings=remappings),

        # --- neo_localization2 node (replaces nav2_amcl) ---
        Node(
            package='neo_localization2',
            executable='neo_localization_node',   # verify with: ros2 pkg executables neo_localization2
            name='neo_localization_node',
            namespace=namespace,
            output='screen',
            parameters=[configured_params],
            remappings=remappings),

        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            namespace=namespace,
            name='lifecycle_manager_localization',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time},
                        {'autostart': autostart},
                        {'node_names': lifecycle_nodes}])
    ])