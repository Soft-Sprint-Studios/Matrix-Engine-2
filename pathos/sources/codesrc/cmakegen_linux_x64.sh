#!/bin/bash

set -e

echo "Installing Linux dependencies..."
sudo apt-get update
sudo apt-get install -y \
    clang \
    cmake \
    libgl1-mesa-dev \
    libx11-dev \
    libxext-dev \
    libxinerama-dev \
    libxrandr-dev \
    libxi-dev \
    libxcursor-dev \
    libfontconfig1-dev \
    libwayland-dev \
    libegl1-mesa-dev

if [ ! -d "build_linux_x64" ]; then
    mkdir build_linux_x64
fi

cd build_linux_x64

echo "Configuring CMake..."
cmake -S .. -B . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++

echo "Building..."
cmake --build . --parallel "$(nproc)"

echo "Build complete."