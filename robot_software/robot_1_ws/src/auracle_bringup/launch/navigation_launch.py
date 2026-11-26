# Copyright (c) 2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, SetEnvironmentVariable
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml


def generate_launch_description():
    # Get the launch directory
    bringup_dir = get_package_share_directory('auracle_bringup')

    namespace = LaunchConfiguration('namespace')
    use_sim_time = LaunchConfiguration('use_sim_time')
    autostart = LaunchConfiguration('autostart')
    params_file = LaunchConfiguration('params_file')
    default_bt_xml_filename = LaunchConfiguration('default_bt_xml_filename')
    map_subscribe_transient_local = LaunchConfiguration('map_subscribe_transient_local')
    use_collision_monitor = LaunchConfiguration('use_collision_monitor')

    # With the collision monitor on, Nav2 publishes to cmd_vel_nav_raw and the monitor republishes
    # to cmd_vel_nav, which is what twist_mux already listens to (twist_mux.yaml is unchanged).
    # With it off, Nav2 publishes straight to cmd_vel_nav, exactly as before.
    nav_cmd_vel = PythonExpression(
        ["'cmd_vel_nav_raw' if '", use_collision_monitor, "'.lower() == 'true' else 'cmd_vel_nav'"])

    lifecycle_nodes = ['controller_server',
                   'planner_server',
                   'behavior_server',
                   'bt_navigator',
                   'waypoint_follower']

    # Map fully qualified names to relative ones so the node's namespace can be prepended.
    # In case of the transforms (tf), currently, there doesn't seem to be a better alternative
    # https://github.com/ros/geometry2/issues/32
    # https://github.com/ros/robot_state_publisher/pull/30
    # TODO(orduno) Substitute with `PushNodeRemapping`
    #              https://github.com/ros2/launch_ros/issues/56
    remappings = [('/tf', 'tf'),
                  ('/tf_static', 'tf_static')]

    # Create our own temporary YAML files that include substitutions
    # The costmap nodes live in the namespace <ns>/local_costmap and <ns>/global_costmap, so a RELATIVE
    # observation topic ('scan_filtered') resolves to <ns>/local_costmap/scan_filtered, which nobody
    # publishes: the obstacle layers were blind. Every 'topic:' key in nav2_params.yaml is a scan source
    # (both costmaps and the collision monitor), so rewrite them all to the absolute name.
    scan_topic = PythonExpression(
        ["('/' + '", namespace, "'.strip('/') + '/scan_filtered') if '", namespace,
         "'.strip('/') else '/scan_filtered'"])

    param_substitutions = {
        'use_sim_time': use_sim_time,
        'default_bt_xml_filename': default_bt_xml_filename,
        'autostart': autostart,
        'topic': scan_topic}

    configured_params = RewrittenYaml(
            source_file=params_file,
            root_key=namespace,
            param_rewrites=param_substitutions,
            convert_types=True)

    return LaunchDescription([
        # Set env var to print messages to stdout immediately
        SetEnvironmentVariable('RCUTILS_LOGGING_BUFFERED_STREAM', '1'),

        DeclareLaunchArgument(
            'namespace', default_value='',
            description='Top-level namespace'),

        DeclareLaunchArgument(
            'use_sim_time', default_value='false',
            description='Use simulation (Gazebo) clock if true'),

        DeclareLaunchArgument(
            'autostart', default_value='true',
            description='Automatically startup the nav2 stack'),

        DeclareLaunchArgument(
            'params_file',
            default_value=os.path.join(bringup_dir, 'config', 'nav2_params.yaml'),
            description='Full path to the ROS2 parameters file to use'),

        DeclareLaunchArgument(
        'default_bt_xml_filename',
        default_value=os.path.join(
            get_package_share_directory('nav2_bt_navigator'),
            'behavior_trees', 'navigate_to_pose_w_replanning_and_recovery.xml'),
        description='Full path to the behavior tree xml file to use'),

        DeclareLaunchArgument(
            'use_collision_monitor', default_value='true',
            description='Run nav2_collision_monitor between Nav2 and twist_mux (hard stop on the live scan). '
                        'false = the old direct path'),

        DeclareLaunchArgument(
            'map_subscribe_transient_local', default_value='false',
            description='Whether to set the map subscriber QoS to transient local'),

        Node(
            package='nav2_controller',
            executable='controller_server',
            namespace=namespace,
            output='screen',
            parameters=[configured_params, {'use_sim_time': use_sim_time}],
            remappings=remappings + [('cmd_vel', nav_cmd_vel)],
            ),

        Node(
            package='nav2_planner',
            executable='planner_server',
            name='planner_server',
            namespace=namespace,
            output='screen',
            parameters=[configured_params, {'use_sim_time': use_sim_time}],
            remappings=remappings),

        Node(
            package='nav2_behaviors',
            executable='behavior_server',
            name='behavior_server',
            namespace=namespace,
            output='screen',
            parameters=[configured_params, {'use_sim_time': use_sim_time}],
            # Spin / BackUp / DriveOnHeading must reach twist_mux too, or recoveries never move the robot
            remappings=remappings + [('cmd_vel', nav_cmd_vel)]),

        Node(
            package='nav2_bt_navigator',
            executable='bt_navigator',
            name='bt_navigator',
            namespace=namespace,
            output='screen',
            parameters=[configured_params, {'use_sim_time': use_sim_time}],
            remappings=remappings),

        Node(
            package='nav2_waypoint_follower',
            executable='waypoint_follower',
            name='waypoint_follower',
            namespace=namespace,
            output='screen',
            parameters=[configured_params, {'use_sim_time': use_sim_time}],
            remappings=remappings),

        # Hard stop on the live filtered scan, independent of the costmaps (config: collision_monitor
        # in nav2_params.yaml). Own lifecycle manager so the Nav2 node list above is untouched.
        Node(
            package='nav2_collision_monitor',
            executable='collision_monitor',
            name='collision_monitor',
            namespace=namespace,
            output='screen',
            parameters=[configured_params, {'use_sim_time': use_sim_time}],
            remappings=remappings,
            condition=IfCondition(use_collision_monitor)),

        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            namespace=namespace,
            name='lifecycle_manager_safety',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time},
                        {'autostart': autostart},
                        {'node_names': ['collision_monitor']},
                        {'bond_timeout': 10.0}],
            condition=IfCondition(use_collision_monitor)),

        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            namespace=namespace,
            name='lifecycle_manager_navigation',
            output='screen',
            parameters=[{'use_sim_time': use_sim_time},
                        {'autostart': autostart},
                        {'node_names': lifecycle_nodes},
                        # default 4 s: a Pi at 100% CPU / thermal throttling can miss it, and the
                        # manager then tears the Nav2 stack down and restarts it.
                        {'bond_timeout': 10.0}]),

    ])