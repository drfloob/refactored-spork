cmake_minimum_required (VERSION 2.6)
project ("Insight Coding Challenge")

## System dependencies are found with CMake's conventions
find_package(Boost REQUIRED COMPONENTS date_time filesystem)
include_directories(${Boost_INCLUDE_DIRS} incl/)

add_executable(Exploration src/naive_exploration.cpp)

set(JsonCpp_SOURCES "incl/jsoncpp.cpp" "incl/json/json.h" "incl/json/json-forwards.h")
add_library(JsonCpp ${JsonCpp_SOURCES})
set_property(TARGET JsonCpp PROPERTY FOLDER "contrib")

target_link_libraries(Exploration JsonCpp ${Boost_LIBRARIES})
