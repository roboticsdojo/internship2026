import os
import tempfile

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _rviz(context, *args, **kwargs):
    ns = context.perform_substitution(LaunchConfiguration('namespace')).strip('/')
    sim = context.perform_substitution(LaunchConfiguration('use_sim_time')).lower() == 'true'
    pkg = get_package_share_directory('auracle_bt')
    with open(os.path.join(pkg, 'rviz', 'mission.rviz')) as f:
        text = f.read().replace('__NS__', f'/{ns}' if ns else '')
    cfg = os.path.join(tempfile.gettempdir(), f'auracle_mission_{ns or "root"}.rviz')
    with open(cfg, 'w') as f:
        f.write(text)
    return [Node(
        package='rviz2', executable='rviz2', name='rviz2', namespace=ns,
        arguments=['-d', cfg],
        remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')],
        parameters=[{'use_sim_time': sim}],
        output='screen')]


def generate_launch_description():
    """RViz only (laptop) for the behaviour-tree mission:

        ros2 launch auracle_bt mission_rviz.launch.py

    Same layout as auracle_bringup's auracle.rviz, but 2D Goal Pose publishes to
    /<namespace>/mission_goal, which bt_runner listens to. Also has the Publish Point tool (-> /<namespace>/clicked_point) for mark mode.
    Use 2D Pose Estimate first, then (goal_tree only) 2D Goal Pose."""
    return LaunchDescription([
        DeclareLaunchArgument('namespace', default_value='auracle'),
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        OpaqueFunction(function=_rviz),
    ])