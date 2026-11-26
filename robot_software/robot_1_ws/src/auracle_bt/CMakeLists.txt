cmake_minimum_required(VERSION 3.8)
project(auracle_bt)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(behaviortree_cpp REQUIRED)
find_package(yaml-cpp REQUIRED)

add_executable(bt_runner src/bt_runner.cpp)
target_compile_features(bt_runner PUBLIC cxx_std_17)
# behaviortree_cpp goes through ament_target_dependencies, like Nav2 does: the imported target is
# named differently between BehaviorTree.CPP packagings, so "BT::behaviortree_cpp" may not exist.
ament_target_dependencies(bt_runner
  rclcpp rclcpp_action std_msgs geometry_msgs nav2_msgs tf2 tf2_ros ament_index_cpp behaviortree_cpp)
target_link_libraries(bt_runner yaml-cpp)
if(TARGET BT::behaviortree_cpp)
  target_link_libraries(bt_runner BT::behaviortree_cpp)
endif()

install(TARGETS bt_runner DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY behavior_trees config launch rviz DESTINATION share/${PROJECT_NAME})

ament_package()