cmake_minimum_required(VERSION 3.8)
project(hailo_yolo_detector)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
#find_package(OpenCV REQUIRED)

# --- HailoRT ---
# The 'hailort' apt package installs headers under /usr/include/hailo and
# libhailort.so under the standard multiarch lib path. There is no official
# CMake config package for HailoRT, so we locate it manually.
find_path(HAILORT_INCLUDE_DIR
  NAMES hailo/hailort.hpp
  PATHS /usr/include /usr/local/include
)
find_library(HAILORT_LIB
  NAMES hailort
  PATHS /usr/lib /usr/lib/aarch64-linux-gnu /usr/local/lib
)
if(NOT HAILORT_INCLUDE_DIR OR NOT HAILORT_LIB)
  message(FATAL_ERROR
    "HailoRT not found. Confirm the 'hailort' apt package (with -dev/headers) "
    "is installed and its version matches your driver (check: hailortcli fw-control identify).")
endif()
message(STATUS "Found HailoRT headers: ${HAILORT_INCLUDE_DIR}")
message(STATUS "Found HailoRT lib: ${HAILORT_LIB}")

add_executable(hailo_detector_node src/hailo_detector_node.cpp)

target_include_directories(hailo_detector_node PRIVATE ${HAILORT_INCLUDE_DIR})
ament_target_dependencies(hailo_detector_node
  rclcpp
  sensor_msgs
  vision_msgs
  cv_bridge
  image_transport
#  OpenCV
)
target_link_libraries(hailo_detector_node ${HAILORT_LIB})

install(TARGETS hailo_detector_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
