#!/bin/bash

echo "Building and running 2D Game Template with SDL2 controller support..."

# Check if user wants direct compilation or CMake is not available
if [ "$1" = "--direct" ] || ! command -v cmake &> /dev/null; then
    if [ "$1" = "--direct" ]; then
        echo "Using direct g++ compilation (user requested)..."
    else
        echo "CMake not found, using direct g++ compilation..."
    fi
    
    # Compile with SDL2 for controller support
    g++ -o game main.cpp -lraylib -lSDL2 -lGL -lm -lpthread -ldl -lrt -lX11
    
    if [ $? -eq 0 ]; then
        echo "Build successful! Running game..."
        ./game
    else
        echo "Build failed!"
        exit 1
    fi
else
    echo "Using CMake build system (default)..."
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Configure and build
    cmake ..
    make
    
    if [ $? -eq 0 ]; then
        echo "CMake build successful! Running game..."
        ./game
    else
        echo "CMake build failed!"
        exit 1
    fi
fi