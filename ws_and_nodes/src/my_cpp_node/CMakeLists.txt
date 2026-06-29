cmake_minimum_required(VERSION 3.8)
project(my_cpp_node)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# -------------------------
# Node 1 (your old node)
# -------------------------
add_executable(my_node src/my_node.cpp)
ament_target_dependencies(my_node rclcpp std_msgs)

# -------------------------
# Node 2 (publisher)
# -------------------------
add_executable(publisher_node src/publisher_node.cpp)
ament_target_dependencies(publisher_node rclcpp std_msgs)

# -------------------------
# Node 3 (subscriber)
# -------------------------
add_executable(subscriber_node src/subscriber_node.cpp)
ament_target_dependencies(subscriber_node rclcpp std_msgs)

# -------------------------
# Install all nodes
# -------------------------
install(TARGETS
  my_node
  publisher_node
  subscriber_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
