#!/bin/bash

echo "Building and running 2D Game Template with SDL2 controller support on macOS..."

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "ERROR: Homebrew not found. Please install Homebrew first:"
    echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    exit 1
fi

# Check if required packages are installed
if ! brew list raylib &> /dev/null; then
    echo "Installing raylib..."
    brew install raylib
fi

if ! brew list sdl2 &> /dev/null; then
    echo "Installing SDL2..."
    brew install sdl2
fi

if ! command -v cmake &> /dev/null; then
    echo "Installing cmake..."
    brew install cmake
fi

if ! command -v g++ &> /dev/null; then
    echo "Installing gcc..."
    brew install gcc
fi

echo "Using CMake build system (recommended)..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -G "Unix Makefiles"
if [ $? -ne 0 ]; then
    echo "CMake configuration failed. Please ensure raylib and SDL2 are installed:"
    echo "  brew install raylib sdl2 cmake gcc"
    exit 1
fi

# Build
make
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "CMake build successful! Running game..."
./game

cd .. 