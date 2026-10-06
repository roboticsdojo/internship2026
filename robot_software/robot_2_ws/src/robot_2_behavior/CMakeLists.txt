cmake_minimum_required(VERSION 3.8)
project(robot_2_behavior)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(behaviortree_cpp REQUIRED)

add_executable(mission_bt src/mission_bt.cpp)
target_compile_features(mission_bt PUBLIC cxx_std_17)
ament_target_dependencies(mission_bt rclcpp rclcpp_action nav2_msgs geometry_msgs behaviortree_cpp)

install(TARGETS mission_bt DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY trees launch DESTINATION share/${PROJECT_NAME})

ament_package()