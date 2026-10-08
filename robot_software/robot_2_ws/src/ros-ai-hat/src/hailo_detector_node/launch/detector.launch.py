from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('hef_path', default_value='yolov8n.hef'),
        DeclareLaunchArgument('input_topic', default_value='/camera/image_raw'),
        DeclareLaunchArgument('output_topic', default_value='/hailo/detections'),
        DeclareLaunchArgument('score_threshold', default_value='0.5'),

        Node(
            package='hailo_yolo_detector',
            executable='hailo_detector_node',
            name='hailo_detector_node',
            output='screen',
            parameters=[{
                'hef_path': LaunchConfiguration('hef_path'),
                'input_topic': LaunchConfiguration('input_topic'),
                'output_topic': LaunchConfiguration('output_topic'),
                'score_threshold': LaunchConfiguration('score_threshold'),
            }],
        ),
    ])

# ros2 launch hailo_yolo_detector detector.launch.py \
 # hef_path:=/home/smartai/rpi5-hailo8l/models/yolov8n.hef \
 # input_topic:=/camera/image_raw