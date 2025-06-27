@echo off
echo Building and running 2D Game Template with SDL2 controller support...

REM Check if CMake is available
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found. Please install CMake and add it to your PATH.
    pause
    exit /b 1
)

REM Check if Visual Studio or MinGW is available
where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Using Visual Studio build system...
    set BUILD_SYSTEM=VS
) else (
    where g++ >nul 2>nul
    if %errorlevel% equ 0 (
        echo Using MinGW build system...
        set BUILD_SYSTEM=MinGW
    ) else (
        echo ERROR: No C++ compiler found. Please install Visual Studio or MinGW.
        pause
        exit /b 1
    )
)

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
if "%BUILD_SYSTEM%"=="VS" (
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo CMake configuration failed. Trying older Visual Studio version...
        cmake .. -G "Visual Studio 16 2019" -A x64
    )
    if %errorlevel% neq 0 (
        echo CMake configuration failed. Please ensure raylib and SDL2 are installed.
        pause
        exit /b 1
    )
    
    REM Build with MSBuild
    cmake --build . --config Release
    if %errorlevel% neq 0 (
        echo Build failed!
        pause
        exit /b 1
    )
    
    echo Visual Studio build successful! Running game...
    cd Release
    game.exe
) else (
    REM MinGW build
    cmake .. -G "MinGW Makefiles"
    if %errorlevel% neq 0 (
        echo CMake configuration failed. Please ensure raylib and SDL2 are installed.
        pause
        exit /b 1
    )
    
    mingw32-make
    if %errorlevel% neq 0 (
        echo Build failed!
        pause
        exit /b 1
    )
    
    echo MinGW build successful! Running game...
    game.exe
)

cd ..
pause 