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

# --- Function to download ONNX Runtime ---
function(download_onnxruntime)
    set(ORT_VERSION "1.21.0")
    set(ORT_DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
    
    # Create the download directory if it doesn't exist
    if(NOT EXISTS "${ORT_DOWNLOAD_DIR}")
        file(MAKE_DIRECTORY "${ORT_DOWNLOAD_DIR}")
    endif()
    
    message(STATUS "Downloading ONNX Runtime v${ORT_VERSION}...")
    
    # Define the download URL for Windows x64
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-win-x64-${ORT_VERSION}.zip")
    set(ORT_DOWNLOAD_PATH "${ORT_DOWNLOAD_DIR}/onnxruntime-win-x64-${ORT_VERSION}.zip")
    set(ORT_EXTRACT_DIR "${ORT_DOWNLOAD_DIR}/onnxruntime-win-x64-${ORT_VERSION}")
    
    # Download the package
    if(NOT EXISTS "${ORT_DOWNLOAD_PATH}")
        message(STATUS "Downloading from ${ORT_URL}...")
        file(DOWNLOAD "${ORT_URL}" "${ORT_DOWNLOAD_PATH}" SHOW_PROGRESS STATUS DOWNLOAD_STATUS)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ONNX Runtime. Status code: ${STATUS_CODE}")
        endif()
    else()
        message(STATUS "Using already downloaded archive at ${ORT_DOWNLOAD_PATH}")
    endif()
    
    # Extract the package using PowerShell (Windows) and handle nested folders
    if(NOT EXISTS "${ORT_EXTRACT_DIR}")
        message(STATUS "Extracting ${ORT_DOWNLOAD_PATH}...")
        # Create the extraction directory
        file(MAKE_DIRECTORY "${ORT_EXTRACT_DIR}")
        
        # First extract to a temporary directory
        set(TEMP_EXTRACT_DIR "${ORT_DOWNLOAD_DIR}/temp_extract")
        file(REMOVE_RECURSE "${TEMP_EXTRACT_DIR}")
        file(MAKE_DIRECTORY "${TEMP_EXTRACT_DIR}")
        
        execute_process(
            COMMAND powershell -Command "Expand-Archive -Path '${ORT_DOWNLOAD_PATH}' -DestinationPath '${TEMP_EXTRACT_DIR}' -Force"
            RESULT_VARIABLE EXTRACT_RESULT
        )
        
        if(NOT EXTRACT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to extract ONNX Runtime package.")
        endif()
        
        # Check if there's a nested folder with the same name and move its contents
        set(NESTED_DIR "${TEMP_EXTRACT_DIR}/onnxruntime-win-x64-${ORT_VERSION}")
        if(EXISTS "${NESTED_DIR}")
            message(STATUS "Fixing nested folder structure...")
            execute_process(
                COMMAND powershell -Command "Get-ChildItem -Path '${NESTED_DIR}' | Copy-Item -Destination '${ORT_EXTRACT_DIR}' -Recurse -Force"
                RESULT_VARIABLE MOVE_RESULT
            )
            
            if(NOT MOVE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to fix nested folder structure.")
            endif()
        else()
            execute_process(
                COMMAND powershell -Command "Get-ChildItem -Path '${TEMP_EXTRACT_DIR}' | Copy-Item -Destination '${ORT_EXTRACT_DIR}' -Recurse -Force"
                RESULT_VARIABLE MOVE_RESULT
            )
            
            if(NOT MOVE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to move extracted files.")
            endif()
        endif()
        
        # Clean up temporary directory
        file(REMOVE_RECURSE "${TEMP_EXTRACT_DIR}")
        
        # Delete the zip file now that we've extracted it
        file(REMOVE "${ORT_DOWNLOAD_PATH}")
        message(STATUS "Removed zip file after successful extraction")
    endif()
    
    # Set the ONNXRUNTIME_ROOT variable in parent scope
    set(ONNXRUNTIME_ROOT "${ORT_EXTRACT_DIR}" PARENT_SCOPE)
    message(STATUS "ONNX Runtime downloaded and extracted to ${ORT_EXTRACT_DIR}")
endfunction()

# --- Find ONNX Runtime ---
set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime-win-x64-1.21.0")

# Check if ONNX Runtime is available, download if not
if(NOT DEFINED ENV{ONNXRUNTIME_ROOT} AND (NOT DEFINED ONNXRUNTIME_ROOT OR NOT EXISTS "${ONNXRUNTIME_ROOT}"))
    message(STATUS "ONNX Runtime not found. Attempting to download...")
    download_onnxruntime()
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

# --- Function to copy ONNX Runtime DLLs to output directory ---
function(copy_ort_dlls target_name)
  if(MSVC)
      message(STATUS "Searching for ONNX Runtime DLLs...")
      
      # Define multiple possible DLL locations to check
      set(POSSIBLE_DLL_DIRS
          "${ORT_ROOT}/bin"
          "${ORT_ROOT}/lib"
          "${ORT_ROOT}/bin/Release"
          "${ORT_ROOT}/bin/Debug"
          "${ORT_ROOT}/redist"
          "${ORT_ROOT}"
      )
      
      set(FOUND_DLLS FALSE)
      foreach(DIR ${POSSIBLE_DLL_DIRS})
          if(EXISTS "${DIR}" AND NOT FOUND_DLLS)
              message(STATUS "Checking directory: ${DIR}")
              file(GLOB POTENTIAL_DLLS "${DIR}/*.dll")
              if(POTENTIAL_DLLS)
                  set(ORT_DLLS ${POTENTIAL_DLLS})
                  set(FOUND_DLLS TRUE)
                  message(STATUS "Found DLLs in: ${DIR}")
                  break()
              endif()
          endif()
      endforeach()
      
      # If no DLLs found, try a recursive search as last resort
      if(NOT FOUND_DLLS)
          message(STATUS "No DLLs found in standard locations, trying recursive search...")
          file(GLOB_RECURSE POTENTIAL_DLLS "${ORT_ROOT}/*.dll")
          if(POTENTIAL_DLLS)
              set(ORT_DLLS ${POTENTIAL_DLLS})
              set(FOUND_DLLS TRUE)
              message(STATUS "Found DLLs using recursive search")
          endif()
      endif()
      
      # Process found DLLs
      if(FOUND_DLLS)
          message(STATUS "Found ${CMAKE_SIZEOF_VOID_P}-bit ONNX Runtime DLLs:")
          foreach(ORT_DLL ${ORT_DLLS})
              message(STATUS "  - ${ORT_DLL}")
              add_custom_command(TARGET ${target_name} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ORT_DLL}" "$<TARGET_FILE_DIR:${target_name}>" COMMENT "Copying ${ORT_DLL}")
          endforeach()
      else()
          message(WARNING "Could not find ONNX Runtime DLLs. You may need to manually copy them to the output directory.")
          # Changed to warning instead of error to allow build to continue
      endif()
  endif()
endfunction()
# --- Find Source Files ---
file(GLOB SOURCE_FILES "src/*.cpp")

# --- Include Directories ---
include_directories(
    include
    ${ONNXRUNTIME_INCLUDE_DIR} # Include ONNX Runtime headers
)

# --- Add Executable Target ---
add_executable(${EXECUTABLE_NAME} ${SOURCE_FILES})

# Copy DLLs for the executable
copy_ort_dlls(${EXECUTABLE_NAME})

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