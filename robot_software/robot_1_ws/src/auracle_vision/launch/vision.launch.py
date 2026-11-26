"""Camera + waste detector on the Pi, in the robot's namespace.

    ros2 launch auracle_vision vision.launch.py                  # normally started by mission.launch.py vision:=true
    ros2 launch auracle_vision vision.launch.py always_on:=true  # bench test without the BT
    ros2 launch auracle_vision vision.launch.py always_on:=true save_dir:=$HOME/screen_frames   # collect data

Topics (namespace auracle): camera/image_raw, vision/enable, vision/decision, vision/detections, vision/status
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    ns = LaunchConfiguration('namespace')
    arg = LaunchConfiguration
    return LaunchDescription([
        DeclareLaunchArgument('namespace', default_value='auracle'),
        DeclareLaunchArgument('model', default_value='waste_yolo26n_320.onnx',
                              description='waste_yolo26n.onnx (640 px, original), waste_yolo26n_320.onnx, '
                                          'waste_yolo26n_320_int8.onnx, or a full path'),
        DeclareLaunchArgument('always_on', default_value='false'),
        DeclareLaunchArgument('save_dir', default_value=''),
        DeclareLaunchArgument('clahe', default_value='false'),
        DeclareLaunchArgument('start_camera', default_value='true'),
        # 640x480 is plenty: the network sees 320x320 anyway. Fewer pixels = less USB/ISP/convert work.
        Node(package='camera_ros', executable='camera_node', name='camera', namespace=ns,
             parameters=[{'width': 640, 'height': 480}],
             condition=IfCondition(arg('start_camera'))),
        Node(package='auracle_vision', executable='waste_detector', name='waste_detector',
             namespace=ns, output='screen',
             parameters=[{
                 'model': arg('model'),
                 'always_on': ParameterValue(arg('always_on'), value_type=bool),
                 'save_dir': arg('save_dir'),
                 'clahe': ParameterValue(arg('clahe'), value_type=bool),
                 'image_topic': 'camera/image_raw',
             }]),
    ])