cmake_minimum_required(VERSION 3.8)
project(pub_sub_pack_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

set(dependencies
  rclcpp
  std_msgs
)

add_executable(min_cpp_pub src/my_publisher.cpp)
ament_target_dependencies(min_cpp_pub ${dependencies})

add_executable(min_cpp_sub src/my_subscriber.cpp)
ament_target_dependencies(min_cpp_sub ${dependencies})

install(
  DIRECTORY include/
  DESTINATION include
)
#install cpp executables and moves the finished programs into a folder where the ros2 run command can find them.
install(
  TARGETS
  min_cpp_pub
  min_cpp_sub
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
