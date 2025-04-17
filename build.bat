@echo off
setlocal

:: --- Configuration ---
set BUILD_DIR=build
set CMAKE_GENERATOR="Visual Studio 17 2022"
set CMAKE_ARCHITECTURE=x64
set BUILD_CONFIG=Release

:: --- Try to initialize Visual Studio environment if needed ---
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo Visual Studio compiler not found, attempting to initialize...
    
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else (
        echo ERROR: Could not find Visual Studio installation.
        pause
        exit /b 1
    )
)

:: --- Check for CMake ---
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found in PATH. Please install CMake.
    pause
    exit /b 1
)

:: --- Clean and create build directory ---
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

:: --- Run CMake ---
cmake .. -G %CMAKE_GENERATOR% -A %CMAKE_ARCHITECTURE%
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed.
    cd ..
    pause
    exit /b 1
)

:: --- Build project ---
cmake --build . --config %BUILD_CONFIG% --verbose
if %errorlevel% neq 0 (
    echo ERROR: Build failed.
    cd ..
    pause
    exit /b 1
)

echo Build successful! Executable is in %BUILD_DIR%\%BUILD_CONFIG%
cd ..
pause
exit /b 0