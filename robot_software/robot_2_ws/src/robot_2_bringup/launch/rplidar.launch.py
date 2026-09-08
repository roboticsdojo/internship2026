#here is the launch command
#ros2 launch robot_2_bringup rplidar.launch.py

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():

    return LaunchDescription([
        DeclareLaunchArgument(
            'serial_port',
            default_value='/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0',
            description='Serial port for the RPLidar'
        ),

        DeclareLaunchArgument(
            'serial_baudrate',
            default_value='115200',
            description='Baudrate for the RPLidar serial connection'
        ),
        Node(
            package='rplidar_ros',
            executable='rplidar_composition',
            namespace = 'robot_2',
            output='screen',
            parameters=[{
                'serial_port': LaunchConfiguration('serial_port'),
                'serial_baudrate': LaunchConfiguration('serial_baudrate'),
                'frame_id': 'laser_frame',
                'angle_compensate': True,
                'scan_mode': 'Standard'
            }],
        )
    ])