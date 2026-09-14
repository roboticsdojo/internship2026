cmake_minimum_required(VERSION 3.8)
project(auracle_hardware)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)

# LibSerial (crayzeewulf/libserial, installed via apt as libserial-dev) ships
# only a pkg-config file, not a CMake package config - so it's located via
# PkgConfig rather than find_package().
find_package(PkgConfig REQUIRED)
pkg_check_modules(SERIAL REQUIRED libserial)

add_library(${PROJECT_NAME} SHARED
  src/robot_system.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${SERIAL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} ${SERIAL_LIBRARIES})
target_link_directories(${PROJECT_NAME} PUBLIC ${SERIAL_LIBRARY_DIRS})

ament_target_dependencies(${PROJECT_NAME}
  rclcpp
  rclcpp_lifecycle
  hardware_interface
  pluginlib
)

# Registers auracle_hardware/AuracleHardwareInterface with pluginlib so
# ros2_control.xacro's <plugin>auracle_hardware/AuracleHardwareInterface</plugin>
# can actually find it at runtime.
pluginlib_export_plugin_description_file(hardware_interface auracle_hardware.xml)

install(
  DIRECTORY include/
  DESTINATION include
)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(export_${PROJECT_NAME})
ament_export_dependencies(rclcpp rclcpp_lifecycle hardware_interface pluginlib)

ament_package()
