# ros2 nodes simple publisher and subscriber node
 
 the following readme shows the steps followed in creating the nodes 
 can be used for learning purposes

## Contents
- Python publisher node
- Python subscriber node
- C++ nodes (publisher/subscriber)

## creating the py pub and sub node

  open the ubuntu 24.04 terminal and run:

      code .

  this opens vs code 

  step 1:  Install colcon by running
   
    
    Sudo apt install colcon
    sudo apt install python3-colcon-common-extensions
  
    N/B run the commands separately

  step 2: create a workspace with a src folder in

    mkdir -p ~/ros2_ws/src
  
  step 3: navigate to the workspace
    
    cd ~/ros2_ws

  step 4: create a  python package

    	ros2 pkg create --build-type ament_python my_py_pkg

  step 5: move to the python package
    
    cd ~/ros2_ws/src/my_py_pkg/my_py_pkg

  step 6: Create the Python publisher and subscriber files

    create the publisher and subcriber files:
      
      touch py_pub.py my_sub.py
    
    in py_pub write the publisher code likewise in the my_sub write the subscriber code

  step 7: Edit the setup.py

    go back to the root package root:

      cd ~/ros2_ws/src/my_py/pkg
     
     open:

      code setup.py

    in the code loacte/find

      </>python

      entry_points={
        'console_scripts': [],
      },

    replace it with:
    
      </>python

      entry_points={
        'console_scripts': [
          'py_pub = my_py_pkg.py_pub:main',
          'my_sub = my_py_pkg.my_sub:main',
        ],
      },
  
  step 8:Edit the package.xml

    open the file:
      
      code package.xml
      
    locate this line:

      </>xml
      <test_depend>ament_copyright</test_depend>
      <test_depend>ament_flake8</test_depend>
      <test_depend>ament_pep257</test_depend>
      <test_depend>python3-pytest</test_depend>

    add the following dependencies above the tes dependencies:

      </>xml
      <depend>rclpy</depend>
      <depend>std_msgs</depend>
      
  step 9: Build the workspace

    go to the workspace root 
    
      cd ~/ros2_ws
    
    Build:
      
      colcon build
    
  step 10:source the workspace

    N/B every time you open a new terminal, do 
      
      source ~/ros2_ws/install/setup.bash
    
    for now, run:
    
      source install/setup.bash
    
  step 11: run the publisher

    open new terminal 1
      
      source ~/ros2_ws/install/setup.bash

      ros2 run my_py_pkg py_pub

    you should see:
      Publishing: "Hello 0"
      Publishing: "Hello 1"
      Publishing: "Hello 2"
      ...

  step 13: run subscriber

    Open Terminal 2 and run 

      source ~/ros2_ws/install/setup.bash
      ros2 run my_py_pkg my_sub

    you should see:
    I heard: "Hello 0"
    I heard: "Hello 1"
    I heard: "Hello 2"
    ...

## creating the c++ pub and sub node

  step 1: Go to the workspace source directory

    run:

      cd ~/ros2_ws/src
  
  step 2: Create the c++ package

    run:

      ros2 pkg create --build-type ament_cmake my_cpp_pkg --dependencies rclcpp std_msgs
    
    this creates the package automatically adds the required dependencies
  
  step 3: Create the publisher and subscriber source files
    
    go to the src dir:

      cd src
    
    create the files:

      touch cpp_pub.cpp cpp_sub.cpp
    
  step 4: Write the publisher code in cpp_pub.cpp

    open:

      code cpp_pub.cpp

    write the cpp code of the publisher

  step 5: write the subscriber code in cpp_sub.cpp

    open:

      code cpp_sub.cpp

    write the cpp subscriber code

  step 6: Editing the cMakeList.txt

    go back to the package root:

      cd ~/ros2_ws/src/my_cpp_pkg

    find the line:

      </>cmake
      find_package(rclcpp REQUIRED)
      find_package(std_msgs REQUIRED)

    now, before:

      ament_package()
    
    add:

      add_executable(cpp_pub src/cpp_pub.cpp)
      ament_target_dependencies(cpp_pub rclcpp std_msgs)

      add_executable(cpp_sub src/cpp_sub.cpp)
      ament_target_dependencies(cpp_sub rclcpp std_msgs)

      install(TARGETS
        cpp_pub
        cpp_sub
        DESTINATION lib/${PROJECT_NAME}
      )
    
    save the file.

  step 7: Build the workspace

    go back to the workspace root:

      cd ~/ros2_ws

    build:
      
      colcon build

  step 8: source the workspace and run the cpp_pub and cpp_sub

    open 2 terminal and source:

      source ~/ros2_ws/install/setup.bash

    in terminal 1 run the publisher:

      ros2 run my_cpp_pkg cpp_pub
    
    you should see:

      [INFO] Publishing: 'Hello 0'
      [INFO] Publishing: 'Hello 1'
      ...

    in terminal 2 run the subcriber

      ros2 run my_cpp_pkg cpp_sub
    
    you should see: 

      [INFO] I heard: 'Hello 0'
      [INFO] I heard: 'Hello 1'
      ... 

That is it.
 Dickson 