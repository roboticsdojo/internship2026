import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node


def generate_launch_description():

    bringup_pkg = get_package_share_directory("robot_2_bringup")
    nav2_bringup_pkg = get_package_share_directory("nav2_bringup")

    nav2_params_file = os.path.join(bringup_pkg, "config", "nav2_params.yaml")
    twist_mux_params_file = os.path.join(bringup_pkg, "config", "twist_mux.yaml")

    default_map = os.path.join(
        bringup_pkg, "config", "maps", "my_map.yaml"
    )

    map_yaml_file = LaunchConfiguration("map")

    declare_map_cmd = DeclareLaunchArgument(
        "map",
        default_value=default_map,
        description="Full path to map yaml file to load",
    )

    # -----------------------------
    # Localization (map_server + amcl)
    # -----------------------------
    # NOTE: navigation_launch.py/localization_launch.py do NOT wrap their
    # Node() actions in a namespace push - passing a 'namespace' launch
    # arg here only affects RewrittenYaml's root_key, not the nodes'
    # actual topic resolution. So these nodes run in the GLOBAL
    # namespace regardless, matching how twist_mux is also run globally
    # (`ros2 run twist_mux twist_mux`, no -r __ns:=). nav2_params.yaml's
    # scan/odom topics are set to explicit /robot_2/... paths to bridge
    # into that global namespace correctly rather than fighting it.
    localization = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_bringup_pkg, "launch", "localization_launch.py")
        ),
        launch_arguments={
            "map": map_yaml_file,
            "params_file": nav2_params_file,
            "use_sim_time": "false",
            "autostart": "true",
        }.items(),
    )

    # -----------------------------
    # Navigation (planner, controller, bt_navigator, etc.)
    # -----------------------------
    navigation = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_bringup_pkg, "launch", "navigation_launch.py")
        ),
        launch_arguments={
            "params_file": nav2_params_file,
            "use_sim_time": "false",
            "autostart": "true",
        }.items(),
    )

    # -----------------------------
    # twist_mux
    # -----------------------------
    # Runs globally (no namespace), matching the manual command this
    # replaces. Subscribes to "cmd_vel" (global) per twist_mux.yaml,
    # which matches collision_monitor's cmd_vel_out_topic ("cmd_vel",
    # also global per nav2_params.yaml) - that link was already correct.
    # Publishes plain (unstamped) Twist to cmd_vel_unstamped - twist_mux
    # does not reliably support TwistStamped output on Jazzy, so a
    # separate twist_stamper node (below) does that conversion instead
    # of relying on twist_mux's finicky 'use_stamped' parameter.
    twist_mux = Node(
        package="twist_mux",
        executable="twist_mux",
        name="twist_mux",
        output="screen",
        parameters=[twist_mux_params_file],
        remappings=[
            ("cmd_vel_out", "cmd_vel_unstamped"),
        ],
    )

    # -----------------------------
    # twist_stamper: bridges twist_mux's unstamped output to the
    # TwistStamped + namespaced topic diff_cont actually expects
    # (Jazzy's diff_drive_controller requires TwistStamped - see
    # launch_robot.launch.py's cmd_vel remap to /robot_2/cmd_vel).
    # -----------------------------
    twist_stamper = Node(
        package="twist_stamper",
        executable="twist_stamper",
        name="twist_stamper",
        output="screen",
        remappings=[
            ("cmd_vel_in", "cmd_vel_unstamped"),
            ("cmd_vel_out", "/robot_2/cmd_vel"),
        ],
    )

    return LaunchDescription(
        [
            declare_map_cmd,
            localization,
            navigation,
            twist_mux,
            twist_stamper,
        ]
    )

    # -------LAUNCH COMMAND--------
    # First, in another terminal, bring up the robot itself:
    #   ros2 launch robot_2_bringup zingira_bringup.launch.py
    # Then, once that's stable, bring up localization/navigation/twist_mux:
    #   ros2 launch robot_2_bringup nav2_bringup.launch.py
    #
    # If you don't have twist_stamper installed yet:
    #   sudo apt install ros-jazzy-twist-stamper