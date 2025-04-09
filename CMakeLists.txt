# Minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C++)
project(CppBacktester VERSION 1.0 LANGUAGES CXX)

# --- Configuration ---
# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Avoid compiler-specific extensions

# --- Executable ---
# Define the name of your executable
set(EXECUTABLE_NAME backtest)

# --- Find Source Files ---
# Use globbing to automatically find all .cpp files in the src directory
# WARNING: If you add/remove source files, you need to re-run CMake
file(GLOB SOURCE_FILES "src/*.cpp")

# --- Include Directories ---
# Tell CMake where to find header files (the 'include' directory)
include_directories(include)

# --- Add Executable Target ---
# Create the executable from the found source files
add_executable(${EXECUTABLE_NAME} ${SOURCE_FILES})

# --- Compiler Flags (Optional but Recommended) ---
# Add compiler warnings and optimization levels
# These are target-specific for better control
if(MSVC)
    # Flags for Microsoft Visual C++ Compiler
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /W4 /WX /O2) # /W4=High warnings, /WX=Warnings as errors, /O2=Optimize
else()
    # Flags for GCC/Clang
    target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall -Wextra -Wpedantic -O3) # Add more warnings, O3=Optimize
endif()

# --- Output Directory (Optional) ---
# Set where the final executable should be placed (e.g., a 'build' directory)
# This keeps your source tree clean
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# If you prefer an 'output' or 'bin' subdirectory within build:
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# --- Installation (Optional) ---
# Define how to install the executable
# install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)

# --- Message ---
message(STATUS "Configuring CppBacktester...")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}") # Often set via CMake GUI or command line (-DCMAKE_BUILD_TYPE=Release)
message(STATUS "Source files: ${SOURCE_FILES}")