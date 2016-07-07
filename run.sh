#!/usr/bin/env bash

# clean & build

if [ -d build ]; then
    rm -r build
fi

mkdir build
cd build
cmake -G "Unix Makefiles" ..
make && ./Exploration
