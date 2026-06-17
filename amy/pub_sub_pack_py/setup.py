from setuptools import find_packages, setup

package_name = 'pub_sub_pack_py'

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
    maintainer='Amy Kibara',
    maintainer_email='amy.kibara@gmail.com',
    description='TODO: Package description',
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            #min_py_pub and min_py_sub are the names of the executables that will be created when the package is built. The format is 'executable_name = package_name.module_name:function_name'
            'min_py_pub = pub_sub_pack_py.my_publisher:main',
            'min_py_sub = pub_sub_pack_py.my_subscriber:main',
        ],
    },
)
