from setuptools import find_packages, setup

package_name = 'rover_power_system'

setup(
    name=package_name,
    version='0.0.0',
    
    # Automatically finds the inner Python folder containing my scripts
    packages=find_packages(exclude=['test']),
    
    data_files=[
        # This blank marker file is required for ROS 2 to officially recognize the package!
        # Note to self: If 'colcon build' ever fails saying "can't copy resource", 
        # it means the blank file in the resource/ folder went missing.
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
            
        # Copies my dependencies list over to the final compiled build
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    
    # Vulcan Rover power system maintainer info
    maintainer='joanouma',
    maintainer_email='joanouma48@gmail.com',
    description='Simulated power telemetry and battery safety triggers for the Mars rover.',
    license='Apache-2.0', 
    
    extras_require={
        'test': [
            'pytest',
        ],
    },
    
    # This is the Master Registry
    # This tells the compiler exactly how to link my terminal commands to my Python code.
    entry_points={
       'console_scripts': [
        # Format -> 'terminal_command = inner_folder.file_name:main_function'
        'battery_publisher = rover_power_system.battery_publisher:main',
        'battery_subscriber = rover_power_system.battery_subscriber:main'
    ],
    },
)