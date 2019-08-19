cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

find_package(gazebo REQUIRED)
include_directories(${GAZEBO_INCLUDE_DIRS} include)
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

add_library(model_push SHARED model_push.cc)
target_link_libraries(model_push ${GAZEBO_LIBRARIES})
add_library(simple_visual SHARED simple_visual.cc)
target_link_libraries(simple_visual ${GAZEBO_LIBRARIES})
add_library(ocean_visual SHARED ocean_visual.cc)
target_link_libraries(ocean_visual ${GAZEBO_LIBRARIES})
