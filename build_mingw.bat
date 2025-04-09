@echo off
setlocal

:: --- Configuration ---
set BUILD_DIR=build
set CMAKE_GENERATOR="MinGW Makefiles"
set LOG_FILE=build_log.txt
set SHOW_OUTPUT=yes
:: Clear log file if it exists
if exist "%LOG_FILE%" del "%LOG_FILE%"
echo Build started at %date% %time% > "%LOG_FILE%"
:: Optional: Set build type (Debug or Release)
:: set CMAKE_BUILD_TYPE=Release
:: set CMAKE_BUILD_TYPE=Debug

:: --- Environment Check (Optional but recommended) ---
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found in PATH. Please install CMake or add it to your PATH.
    echo ERROR: CMake not found in PATH. Please install CMake or add it to your PATH. >> "%LOG_FILE%"
    goto :eof
)

where mingw32-make >nul 2>nul
if %errorlevel% neq 0 (
    echo WARNING: mingw32-make not found in PATH. Trying 'make'... 
    echo WARNING: mingw32-make not found in PATH. Trying 'make'... >> "%LOG_FILE%"
    where make >nul 2>nul
     if %errorlevel% neq 0 (
        echo ERROR: Neither 'mingw32-make' nor 'make' found in PATH. Please ensure MinGW build tools are in your PATH.
        echo You might need to run this script from an MSYS2 MinGW environment shell.
        echo ERROR: Neither 'mingw32-make' nor 'make' found in PATH. >> "%LOG_FILE%"
        goto :eof
    ) else (
       set MAKE_COMMAND=make
    )
) else (
    set MAKE_COMMAND=mingw32-make
)

where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: g++ compiler not found in PATH. Please ensure MinGW g++ is in your PATH.
    echo You might need to run this script from an MSYS2 MinGW environment shell.
    echo ERROR: g++ compiler not found in PATH. >> "%LOG_FILE%"
    goto :eof
)

:: --- Build Steps ---
echo --- Cleaning previous build directory (if exists) ---
if exist "%BUILD_DIR%\" (
    echo Deleting %BUILD_DIR%...
    rmdir /s /q "%BUILD_DIR%"
    if errorlevel 1 (
      echo ERROR: Failed to delete existing build directory. It might be in use.
      echo ERROR: Failed to delete existing build directory. >> "%LOG_FILE%"
      goto :eof
    )
)

echo --- Creating build directory ---
mkdir "%BUILD_DIR%"
if errorlevel 1 (
  echo ERROR: Failed to create build directory '%BUILD_DIR%'.
  echo ERROR: Failed to create build directory '%BUILD_DIR%'. >> "%LOG_FILE%"
  goto :eof
)
cd "%BUILD_DIR%"

echo --- Running CMake ---
:: Construct CMake command with optional build type
set CMAKE_CMD=cmake .. -G %CMAKE_GENERATOR%
if defined CMAKE_BUILD_TYPE (
    set CMAKE_CMD=%CMAKE_CMD% -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE%
)
echo %CMAKE_CMD%
echo %CMAKE_CMD% >> "%LOG_FILE%"

:: Run CMake and capture output for both console and log file
%CMAKE_CMD% > temp_cmake_output.txt 2>&1
type temp_cmake_output.txt
type temp_cmake_output.txt >> "%LOG_FILE%"
if exist temp_cmake_output.txt del temp_cmake_output.txt

if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    cd ..
    goto :eof
)

echo --- Running Make (%MAKE_COMMAND%) ---
echo --- Running Make (%MAKE_COMMAND%) --- >> "%LOG_FILE%"

:: Run Make and capture output for both console and log file
%MAKE_COMMAND% > temp_make_output.txt 2>&1
type temp_make_output.txt
type temp_make_output.txt >> "%LOG_FILE%"
if exist temp_make_output.txt del temp_make_output.txt

if errorlevel 1 (
    echo ERROR: Build failed. See errors above.
    cd ..
    goto :eof
)

echo --- Build Successful ---
echo --- Build Successful --- >> "%LOG_FILE%"
echo Executable located in: %CD%
echo Executable located in: %CD% >> "%LOG_FILE%"
cd ..

:eof
echo Log file is available at: %CD%\%LOG_FILE%
endlocal
echo --- Script Finished ---
echo Check %LOG_FILE% for detailed build output and any errors.
pause