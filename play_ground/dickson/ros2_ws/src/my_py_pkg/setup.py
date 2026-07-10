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
    maintainer='dickson-kabiru',
    maintainer_email='dickson-kabiru@todo.todo',
    description='A ROS 2 Python package implementing a simple publisher and subscriber using rclpy',
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'py_pub = my_py_pkg.py_pub:main',
            'my_sub = my_py_pkg.my_sub:main',
        ],
    },
)