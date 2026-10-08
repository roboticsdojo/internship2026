from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('model_path', default_value=''),
        DeclareLaunchArgument('input_topic', default_value='/camera/image_raw'),
        DeclareLaunchArgument('output_topic', default_value='/cpu/detections'),
        DeclareLaunchArgument('imgsz', default_value='640'),
        DeclareLaunchArgument('score_threshold', default_value='0.5'),
        DeclareLaunchArgument('iou_threshold', default_value='0.45'),
        DeclareLaunchArgument('num_threads', default_value='4'),

        Node(
            package='cpu_yolo_detector',
            executable='cpu_detector_node',
            name='cpu_detector_node',
            output='screen',
            parameters=[{
                'model_path': LaunchConfiguration('model_path'),
                'input_topic': LaunchConfiguration('input_topic'),
                'output_topic': LaunchConfiguration('output_topic'),
                'imgsz': LaunchConfiguration('imgsz'),
                'score_threshold': LaunchConfiguration('score_threshold'),
                'iou_threshold': LaunchConfiguration('iou_threshold'),
                'num_threads': LaunchConfiguration('num_threads'),
            }],
        ),
    ])
