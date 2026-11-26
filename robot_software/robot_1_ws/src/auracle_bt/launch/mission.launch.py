import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    """Mission on the Pi, AFTER launch_robot.launch.py. Park the robot at the mapping start, then:

        ros2 launch auracle_bt mission.launch.py map:=<map.yaml> route:=N

    Starts Nav2 + AMCL on the map (start_nav:=true) and the mission runner. The runner waits for
    Nav2, sets the start pose (0,0,0) itself, drives to load region N, pauses, drives to offload
    region N, returns home, turns back to the start heading and waits for the next region:
        ros2 topic pub --once /auracle/region std_msgs/msg/Int32 "{data: 2}"
    route:=0 (default) starts by waiting for a region. Regions are in config/regions.yaml.

    auto_pose:=false asks for a 2D Pose Estimate click instead (laptop: mission_rviz.launch.py).
    First time per map: mark:=true. Click the centre of LOAD 1,2,3 then OFFLOAD 1,2,3 with the RViz
    Publish Point tool; the runner writes regions.yaml and exits. The robot does not move.
    Click-your-goals mode: tree_file:=goal_tree.xml

    Camera picks the region: vision:=true. Also starts auracle_vision (camera + waste_detector) and
    uses vision_tree.xml: look at the screen -> metal/organic/plastic -> region 1/2/3 (map in the XML).
    vision_model:=waste_yolo26n.onnx goes back to the original 640 px model."""
    bringup = get_package_share_directory('auracle_bringup')
    ns = LaunchConfiguration('namespace')

    nav = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(bringup, 'launch', 'localization_nav_rviz.launch.py')),
        launch_arguments={
            'namespace': ns,
            'use_rviz': 'false',
            'map': LaunchConfiguration('map'),
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'use_collision_monitor': LaunchConfiguration('use_collision_monitor'),
        }.items(),
        condition=IfCondition(LaunchConfiguration('start_nav')),
    )

    vision = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('auracle_vision'), 'launch', 'vision.launch.py')),
        launch_arguments={
            'namespace': ns,
            'model': LaunchConfiguration('vision_model'),
        }.items(),
        condition=IfCondition(LaunchConfiguration('vision')),
    )

    runner = Node(
        package='auracle_bt',
        executable='bt_runner',
        name='bt_runner',
        namespace=ns,
        output='screen',
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
        parameters=[{
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'route': ParameterValue(LaunchConfiguration('route'), value_type=int),
            'mark': ParameterValue(LaunchConfiguration('mark'), value_type=bool),
            'auto_pose': ParameterValue(LaunchConfiguration('auto_pose'), value_type=bool),
            'vision': ParameterValue(LaunchConfiguration('vision'), value_type=bool),
            'route_file': LaunchConfiguration('route_file'),
            'tree_file': LaunchConfiguration('tree_file'),
        }],
    )

    return LaunchDescription([
        DeclareLaunchArgument('route', default_value='0',
                              description='first region to run (1, 2 or 3); 0 = wait for one on the topic region'),
        DeclareLaunchArgument('auto_pose', default_value='true',
                              description='true: robot is parked at the mapping start (0,0,0), no click. false: 2D Pose Estimate in RViz'),
        DeclareLaunchArgument('mark', default_value='false',
                              description='true: record the six regions from Publish Point clicks'),
        DeclareLaunchArgument('map', default_value=os.path.expanduser('~/maps/trial_6a.yaml')),
        DeclareLaunchArgument('use_collision_monitor', default_value='false',
                              description='false (default) = Nav2 drives twist_mux directly, no collision monitor; true = add it'),
        DeclareLaunchArgument('start_nav', default_value='true',
                              description='false if Nav2 is already running'),
        DeclareLaunchArgument('vision', default_value='false',
                              description='true: the camera picks the region (vision_tree.xml + auracle_vision)'),
        DeclareLaunchArgument('vision_model', default_value='waste_yolo26n_320.onnx'),
        DeclareLaunchArgument('namespace', default_value='auracle'),
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('route_file', default_value='',
                              description='empty = share/auracle_bt/config/regions.yaml'),
        DeclareLaunchArgument('tree_file', default_value='',
                              description='empty = region_tree.xml (or mark_tree.xml when mark:=true). Name or full path'),
        nav,
        vision,
        runner,
    ])