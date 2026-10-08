from setuptools import setup

package_name = 'cpu_yolo_detector'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/cpu_detector.launch.py']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer="Lenny Ng'ang'a",
    maintainer_email='codewithlennylen254@gmail.com',
    description='CPU-only YOLOv8 object detection ROS2 node via ONNX Runtime, for benchmarking against hailo_yolo_detector.',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'cpu_detector_node = cpu_yolo_detector.cpu_detector_node:main',
        ],
    },
)
