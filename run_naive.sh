#!/usr/bin/env bash

# clean & build

# if [ -d build ]; then
#     rm -r build
# fi

cat <<EOF > CMakeLists.txt
cmake_minimum_required (VERSION 2.6)
project ("Insight Coding Challenge")

## System dependencies are found with CMake's conventions
find_package(Boost REQUIRED COMPONENTS date_time filesystem)
include_directories(\${Boost_INCLUDE_DIRS})

add_executable(MedianDegreeEngine_NAIVE src/naive_solution.cpp)

set(JsonCpp_SOURCES "src/jsoncpp.cpp" "src/json/json.h" "src/json/json-forwards.h")
add_library(JsonCpp \${JsonCpp_SOURCES})
set_property(TARGET JsonCpp PROPERTY FOLDER "contrib")

target_link_libraries(MedianDegreeEngine_NAIVE JsonCpp \${Boost_LIBRARIES})
EOF

if [ ! -d build ]; then
    mkdir build
fi

cd build
#cmake -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ..
cmake -G "Unix Makefiles" ..
make && ./MedianDegreeEngine_NAIVE
