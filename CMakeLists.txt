cmake_minimum_required(VERSION 3.10)
project(raylib-template)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED raylib)

# Add raylib library directory to linker search path on macOS
if(APPLE)
    link_directories(${RAYLIB_LIBRARY_DIRS})
endif()

# Find SDL2 - try find_package first, then pkg-config
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# Check for Wayland support
pkg_check_modules(WAYLAND QUIET wayland-client wayland-cursor wayland-egl)
if(WAYLAND_FOUND)
    message(STATUS "Wayland support detected")
    add_definitions(-DWAYLAND_SUPPORT)
endif()

# Add executable
add_executable(game main.cpp)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    # Linux
    set(PLATFORM_LIBS GL m pthread dl rt X11)
    
    # Add Wayland libraries if available
    if(WAYLAND_FOUND)
        list(APPEND PLATFORM_LIBS ${WAYLAND_LIBRARIES})
        message(STATUS "Linking with Wayland libraries")
    endif()
elseif(APPLE)
    # macOS
    set(PLATFORM_LIBS "-framework OpenGL" m pthread dl)
elseif(WIN32)
    # Windows
    set(PLATFORM_LIBS opengl32 m)
endif()

# Link libraries
target_link_libraries(game 
    ${RAYLIB_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${PLATFORM_LIBS}
)

# Include directories
target_include_directories(game PRIVATE 
    ${RAYLIB_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
)

# Compiler flags
target_compile_options(game PRIVATE 
    ${RAYLIB_CFLAGS_OTHER}
    ${SDL2_CFLAGS_OTHER}
    ${WAYLAND_CFLAGS_OTHER}
)

# Set output directory
set_target_properties(game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Optional: Set debug/release flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(game PRIVATE -g -O0)
else()
    target_compile_options(game PRIVATE -O2)
endif()

# Print configuration info
message(STATUS "Building raylib-template with SDL2 controller support")
message(STATUS "Raylib version: ${RAYLIB_VERSION}")
if(SDL2_VERSION)
    message(STATUS "SDL2 version: ${SDL2_VERSION}")
else()
    message(STATUS "SDL2 found via find_package")
endif()
if(WAYLAND_FOUND)
    message(STATUS "Wayland support: Enabled")
else()
    message(STATUS "Wayland support: Not detected (will use X11)")
endif()

# Copy resources folder to build directory after build
add_custom_command(
    TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:game>/resources
) 