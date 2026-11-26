from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():

    namespace = LaunchConfiguration('namespace')
    serial_port = LaunchConfiguration('serial_port')

    declare_namespace = DeclareLaunchArgument(
        'namespace', default_value='',
        description='Top-level namespace, matching the rest of the bringup.')

    declare_serial_port = DeclareLaunchArgument(
        'serial_port',
        default_value='/dev/rplidar',
        description=(
            "Serial device for the RPLidar. The previous hardcoded "
            "/dev/serial/by-path/... string was specific to one machine's USB "
            "topology and would not exist on a different Pi. Recommended: add a "
            "udev rule (matching the lidar's USB vendor/product ID) that creates "
            "a stable /dev/rplidar symlink, then leave this at its default. "
            "Otherwise override with e.g. serial_port:=/dev/ttyUSB0."
        )
    )

    rplidar_node = Node(
        package='rplidar_ros',
        executable='rplidar_composition',
        name='rplidar_node',
        namespace=namespace,
        output='screen',
        # The A1 on a Pi USB port drops out now and then (power dip, cable). Without
        # respawn the node exits once and /scan stays silent for the rest of the run.
        respawn=True,
        respawn_delay=2.0,
        parameters=[{
            'serial_port': serial_port,
            'serial_baudrate': 115200,
            'frame_id': 'laser_frame',
            'angle_compensate': True,
            'scan_mode': 'Standard',
        }]
    )

    return LaunchDescription([
        declare_namespace,
        declare_serial_port,
        rplidar_node,
    ])
