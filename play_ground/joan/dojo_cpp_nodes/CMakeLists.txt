cmake_minimum_required(VERSION 3.8)
project(dojo_cpp_nodes)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
#these instructions tell the compiler to convert your .cpp files into runnable executables
# --- 1. BUILD THE PUBLISHER EXECUTABLE ---
add_executable(dojo_pub src/publisher_node.cpp)
ament_target_dependencies(dojo_pub rclcpp std_msgs)

# --- 2. BUILD THE SUBSCRIBER EXECUTABLE ---
add_executable(dojo_sub src/subscriber_node.cpp)
ament_target_dependencies(dojo_sub rclcpp std_msgs)

# --- 3. INSTALL THEM SO THE TERMINAL CAN FIND THEM ---
install(TARGETS
  dojo_pub
  dojo_sub
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
