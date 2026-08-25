cmake_minimum_required(VERSION 3.8)

project(robot_2_hardware)


# ==========================================================
# Compiler settings
# ==========================================================

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# ==========================================================
# Dependencies
# ==========================================================

find_package(ament_cmake REQUIRED)

find_package(hardware_interface REQUIRED)

find_package(pluginlib REQUIRED)

find_package(rclcpp REQUIRED)

find_package(rclcpp_lifecycle REQUIRED)


# ==========================================================
# Hardware interface library
# ==========================================================

add_library(
  robot_2_hardware
  SHARED
  src/robot_2_system.cpp
)


# ==========================================================
# Include directories
# ==========================================================

target_include_directories(
  robot_2_hardware
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)


# ==========================================================
# Dependencies
# ==========================================================

ament_target_dependencies(
  robot_2_hardware

  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
)


# ==========================================================
# Plugin visibility
# ==========================================================

pluginlib_export_plugin_description_file(
  hardware_interface
  robot_2_hardware.xml
)


# ==========================================================
# Install headers
# ==========================================================

install(
  DIRECTORY include/
  DESTINATION include
)


# ==========================================================
# Install library
# ==========================================================

install(
  TARGETS robot_2_hardware
  EXPORT export_robot_2_hardware
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)


# ==========================================================
# Install plugin XML
# ==========================================================

install(
  FILES robot_2_hardware.xml
  DESTINATION share/${PROJECT_NAME}
)


# ==========================================================
# Export dependencies
# ==========================================================

ament_export_targets(
  export_robot_2_hardware
  HAS_LIBRARY_TARGET
)

ament_export_dependencies(
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
)


# ==========================================================
# Package
# ==========================================================

ament_package()