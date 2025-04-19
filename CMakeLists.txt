#  always load vcpkg’s toolchain
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")
endif()

# Minimum CMake version required
cmake_minimum_required(VERSION 3.12)

# Project name and language (C++)
project(CppBacktester VERSION 1.0 LANGUAGES CXX)

# Add cmake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# use vcpkg
find_package(CURL REQUIRED)

# --- Configuration ---
# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Avoid compiler-specific extensions

# --- Build Options ---
option(BUILD_EXECUTABLE "Build the C++ executable target" ON)
option(BUILD_PYTHON_MODULE "Build the Python module target" ON)

# --- Define output directories ---
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/output")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/lib")

# Set VS-specific output directories to avoid intermediate directories
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${OUTPUT_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${OUTPUT_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${OUTPUT_DIR}/lib")
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

# --- Python Development ---
if(BUILD_PYTHON_MODULE)
    find_package(Python REQUIRED COMPONENTS Interpreter Development NumPy)
    message(STATUS "Found Python: ${Python_EXECUTABLE} (found version \"${Python_VERSION}\")")
    message(STATUS "Python include directories: ${Python_INCLUDE_DIRS}")

    # Install Python dependencies automatically
    execute_process(
        COMMAND ${Python_EXECUTABLE} -m pip install --upgrade pip
    )
    execute_process(
        COMMAND ${Python_EXECUTABLE} -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt
    )

    # --- Add pybind11 ---
    add_subdirectory(third_party/pybind11)
endif()

# --- ONNX Runtime Configuration ---
set(ORT_VERSION "1.21.0")
set(ORT_DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# Detect Host Architecture
if(CMAKE_HOST_WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ORT_ARCH "win-x64")
    else()
        set(ORT_ARCH "win-x86")
    endif()
elseif(CMAKE_HOST_APPLE)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOST_ARCH STREQUAL "arm64")
        set(ORT_ARCH "osx-arm64")
    else()
        set(ORT_ARCH "osx-x64")
    endif()
elseif(CMAKE_HOST_UNIX AND NOT CMAKE_HOST_APPLE)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOST_ARCH STREQUAL "aarch64")
        set(ORT_ARCH "linux-aarch64")
    elseif(HOST_ARCH STREQUAL "x86_64")
        set(ORT_ARCH "linux-x64")
    else()
        message(WARNING "Unsupported Linux architecture: ${HOST_ARCH}. Manual setup required.")
        set(ORT_ARCH "linux-unknown")
    endif()
else()
    message(WARNING "Unsupported host OS for automatic ORT download. Manual setup required.")
    set(ORT_ARCH "unknown")
endif()

message(STATUS "ONNX Runtime Architecture Target: ${ORT_ARCH}")

# Determine ONNX Runtime location
set(ORT_ROOT_GUESS "${ORT_DOWNLOAD_DIR}/onnxruntime-${ORT_ARCH}-${ORT_VERSION}")
if(NOT DEFINED ENV{ONNXRUNTIME_ROOT} AND NOT IS_DIRECTORY "${ORT_ROOT_GUESS}")
    message(STATUS "ONNX Runtime not found. Attempting to download...")
    include(DownloadONNX)
    download_onnxruntime(ONNXRUNTIME_ROOT_DOWNLOADED ${ORT_VERSION} ${ORT_ARCH} ${ORT_DOWNLOAD_DIR})
    set(ORT_ROOT ${ONNXRUNTIME_ROOT_DOWNLOADED})
elseif(DEFINED ENV{ONNXRUNTIME_ROOT})
    set(ORT_ROOT $ENV{ONNXRUNTIME_ROOT})
    message(STATUS "Using ONNX Runtime from environment: ${ORT_ROOT}")
else()
    set(ORT_ROOT ${ORT_ROOT_GUESS})
    message(STATUS "Using existing ONNX Runtime at: ${ORT_ROOT}")
endif()

# Find ONNX Runtime include and library files
find_path(ONNXRUNTIME_INCLUDE_DIR NAMES onnxruntime_cxx_api.h
          PATHS "${ORT_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime
             PATHS "${ORT_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)

message(STATUS "Found ONNX Runtime Include: ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "Found ONNX Runtime Library: ${ONNXRUNTIME_LIBRARY}")

# Include shared library copy module
include(CopySharedLibs)

# --- Source Files ---
# Define core library files used by both targets
file(GLOB CORE_SOURCE_FILES CONFIGURE_DEPENDS
    "src/*.cpp"
)

# Explicitly exclude main.cpp and bindings.cpp from core sources
list(FILTER CORE_SOURCE_FILES EXCLUDE REGEX "(main\.cpp|bindings\.cpp)$")

# Define source files for each target
set(EXECUTABLE_MAIN_SRC "src/main.cpp")
set(PYTHON_BINDINGS_SRC "src/bindings.cpp")

# --- Common Compiler Settings ---
set(COMMON_COMPILE_FLAGS_DEBUG "")
set(COMMON_COMPILE_FLAGS_RELEASE "")
if(MSVC)
    list(APPEND COMMON_COMPILE_FLAGS_DEBUG /W4 /WX /Od /MP)
    list(APPEND COMMON_COMPILE_FLAGS_RELEASE /W4 /WX /O2 /MP)
else()
    list(APPEND COMMON_COMPILE_FLAGS_DEBUG -Wall -Wextra -Wpedantic -g -O0)
    list(APPEND COMMON_COMPILE_FLAGS_RELEASE -Wall -Wextra -Wpedantic -O3 -DNDEBUG)
endif()

# --- Executable Target ---
if(BUILD_EXECUTABLE)
    set(EXECUTABLE_NAME "backtest_executable")

    # Create executable target
    add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_MAIN_SRC} ${CORE_SOURCE_FILES})

    # Make sure bindings.cpp is not included in the executable target
    set_source_files_properties(${PYTHON_BINDINGS_SRC} PROPERTIES HEADER_FILE_ONLY TRUE)

    # Setup include directories
    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ONNXRUNTIME_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include/xgboost
        ${Python_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xgboost
    )

    # Link libraries
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11
        ${ONNXRUNTIME_LIBRARY} 
        CURL::libcurl
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xgboost/xgboost.lib
        E:/Python312/libs/python312.lib
    )

    # Set compiler options
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        $<$<CONFIG:Debug>:${COMMON_COMPILE_FLAGS_DEBUG}>
        $<$<CONFIG:Release>:${COMMON_COMPILE_FLAGS_RELEASE}>
    )

    # Copy ORT shared libraries
    copy_ort_shared_libs(
        ${EXECUTABLE_NAME} 
        "${ORT_ROOT}" 
        "${ONNXRUNTIME_LIBRARY}"
        "${OUTPUT_DIR}"
    )
endif()

# --- Python Module Target ---
if(BUILD_PYTHON_MODULE)
    set(MODULE_NAME "cppbacktester_py")

    # Create Python module target
    set_source_files_properties(${PYTHON_BINDINGS_SRC} PROPERTIES HEADER_FILE_ONLY OFF)
    pybind11_add_module(${MODULE_NAME} 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings.cpp
        ${CORE_SOURCE_FILES}
    )

    # Setup include directories
    target_include_directories(${MODULE_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ONNXRUNTIME_INCLUDE_DIR}
        ${Python_INCLUDE_DIRS}
        ${PYBIND11_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xgboost
    )

    # Link libraries
    target_link_libraries(${MODULE_NAME} PRIVATE
        # ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ONNXRUNTIME_LIBRARY}
        ${Python_LIBRARIES}
        CURL::libcurl
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xgboost/xgboost.lib
    )

    # Set compiler options
    target_compile_options(${MODULE_NAME} PRIVATE
        $<$<CONFIG:Debug>:${COMMON_COMPILE_FLAGS_DEBUG}>
        $<$<CONFIG:Release>:${COMMON_COMPILE_FLAGS_RELEASE}>
    )

    # Copy ORT shared libraries
    copy_ort_shared_libs(
        ${MODULE_NAME}
        "${ORT_ROOT}"
        "${ONNXRUNTIME_LIBRARY}"
        "${OUTPUT_DIR}"
    )
endif()