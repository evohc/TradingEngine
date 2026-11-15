#!/usr/bin/env bash
set -e

# Default to Release mode if no argument is passed
BUILD_TYPE=${1:-Release}

if [ "$1" = "clean" ]; then
    if [ -d "build" ]; then
        echo "Cleaning build directory..."
        rm -rf build
        echo "Clean finished."
    else
        echo "Build directory does not exist, clean not required."
    fi
    exit 0
fi

if [ ! -d "build" ]; then
    echo "Creating build directory."
    mkdir -p build
fi

echo "Building in [${BUILD_TYPE}] mode..."

cmake -S . -B build -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build build

echo "Finished building."