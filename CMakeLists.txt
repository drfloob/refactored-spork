cmake_minimum_required (VERSION 2.6)
project ("Insight Coding Challenge")

## System dependencies are found with CMake's conventions
 find_package(Boost REQUIRED COMPONENTS date_time)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  add_executable(Exploration src/naive_exploration.cpp)
  target_link_libraries(Exploration ${Boost_LIBRARIES})
endif()
