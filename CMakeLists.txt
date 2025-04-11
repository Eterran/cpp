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

# --- Find ONNX Runtime ---
# You can set this as an environment variable OR uncomment and set it here:
set(ONNXRUNTIME_ROOT "D:/Program_Files/onnxruntime-osx-universal2-1.21.0") # Example Path

if(NOT DEFINED ENV{ONNXRUNTIME_ROOT} AND NOT DEFINED ONNXRUNTIME_ROOT)
    message(FATAL_ERROR "ONNXRUNTIME_ROOT environment variable or CMake variable is not set. Please point it to your ONNX Runtime installation.")
endif()

# Use the ENV variable if defined, otherwise use the CMake variable
if(DEFINED ENV{ONNXRUNTIME_ROOT})
    set(ORT_ROOT $ENV{ONNXRUNTIME_ROOT})
else()
    set(ORT_ROOT ${ONNXRUNTIME_ROOT})
endif()

message(STATUS "Using ONNX Runtime from: ${ORT_ROOT}")

find_path(ONNXRUNTIME_INCLUDE_DIR NAMES onnxruntime_cxx_api.h PATHS "${ORT_ROOT}/include")
find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime PATHS "${ORT_ROOT}/lib")

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "Could not find ONNX Runtime include or library files in ${ORT_ROOT}")
endif()

message(STATUS "Found ONNX Runtime Include Dir: ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "Found ONNX Runtime Library: ${ONNXRUNTIME_LIBRARY}")

# --- Find Source Files ---
file(GLOB SOURCE_FILES "src/*.cpp")

# --- Include Directories ---
include_directories(
    include
    ${ONNXRUNTIME_INCLUDE_DIR} # Include ONNX Runtime headers
)

# --- Add Executable Target ---
add_executable(${EXECUTABLE_NAME} ${SOURCE_FILES})

# --- Link Libraries ---
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    ${ONNXRUNTIME_LIBRARY}
)

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