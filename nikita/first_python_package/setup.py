# an automatically generated file when creating the python package that contains the package name, version, description, maintainer, license, dependencies, and build type. It also specifies the entry point for the publisher node script.

#it defines how 'setuptools' builds, packages, and installs nodes, scripts, and configurations so the ROS 2 ecosystem can discover and run them
from setuptools import find_packages, setup

package_name = 'first_python_package'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='shiku03',
    maintainer_email='nikki.shiku@gmail.com',
    description='First Python publisher using rclpy',
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [ 'talker = first_python_package.publisher_node:main'
        ],
    },
)
