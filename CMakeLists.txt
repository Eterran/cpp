# Minimum CMake version required
cmake_minimum_required(VERSION 3.12) # Keep the version from File 1

# Project name and language (C++)
project(CppBacktester VERSION 1.0 LANGUAGES CXX)

# --- Configuration ---
# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Avoid compiler-specific extensions

# --- Build Options (Optional: Allow disabling parts) ---
option(BUILD_EXECUTABLE "Build the C++ executable target" ON)
option(BUILD_PYTHON_MODULE "Build the Python module target" ON)

# --- Add pybind11 (Needed only if building the module) ---
if(BUILD_PYTHON_MODULE)
    # Assumes pybind11 is cloned into third_party/pybind11
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11")
        message(FATAL_ERROR "pybind11 not found in 'third_party/pybind11'. Required for Python module. Please clone it there or set BUILD_PYTHON_MODULE=OFF.")
    endif()
    add_subdirectory(third_party/pybind11)
    message(STATUS "Found pybind11 submodule (needed for Python module)")
endif()

# --- ONNX Runtime Handling (Using the more robust logic from File 1) ---
# Define ORT version and directories relative to project source
set(ORT_VERSION "1.21.0") # Consider making this a variable if you might change it
# Detect Host Architecture
if(CMAKE_HOST_WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ORT_ARCH "win-x64")
    else()
        set(ORT_ARCH "win-x86") # Or handle error if unsupported
    endif()
elseif(CMAKE_HOST_APPLE)
    # Basic check for Apple Silicon vs Intel
    execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOST_ARCH STREQUAL "arm64")
        set(ORT_ARCH "osx-arm64")
    else()
        set(ORT_ARCH "osx-x64") # Check if this naming is correct for ORT
    endif()
elseif(CMAKE_HOST_UNIX AND NOT CMAKE_HOST_APPLE)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOST_ARCH STREQUAL "aarch64")
        set(ORT_ARCH "linux-aarch64")
    elseif(HOST_ARCH STREQUAL "x86_64")
        set(ORT_ARCH "linux-x64")
    else()
        message(WARNING "Unsupported Linux architecture for automatic ORT download: ${HOST_ARCH}. Manual setup required.")
        set(ORT_ARCH "linux-unknown") # Fallback
    endif()
else()
    message(WARNING "Unsupported host OS for automatic ORT download. Manual setup required.")
    set(ORT_ARCH "unknown") # Fallback
endif()
message(STATUS "Detected ONNX Runtime Architecture Target: ${ORT_ARCH}")

set(ORT_PACKAGE_NAME "onnxruntime-${ORT_ARCH}-${ORT_VERSION}")
set(ORT_DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(ORT_ROOT_GUESS "${ORT_DOWNLOAD_DIR}/${ORT_PACKAGE_NAME}") # Where it *should* be after extraction

# Function to download ONNX Runtime (Adopted from File 1, added non-Windows extraction)
function(download_onnxruntime output_ort_root_var)
    # Determine appropriate archive format based on OS
    if(WIN32)
        set(ORT_ARCHIVE_SUFFIX "zip")
    else()
        set(ORT_ARCHIVE_SUFFIX "tgz") # Assuming tgz for Linux/macOS, adjust if needed
    endif()
    set(ORT_DOWNLOAD_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-${ORT_ARCH}-${ORT_VERSION}.${ORT_ARCHIVE_SUFFIX}")
    set(ORT_DOWNLOAD_PATH "${ORT_DOWNLOAD_DIR}/${ORT_PACKAGE_NAME}.${ORT_ARCHIVE_SUFFIX}")
    set(ORT_EXTRACT_DIR "${ORT_DOWNLOAD_DIR}/${ORT_PACKAGE_NAME}") # Consistent naming

    message(STATUS "ONNX Runtime target directory: ${ORT_EXTRACT_DIR}")

    # Create the download directory if it doesn't exist
    if(NOT EXISTS "${ORT_DOWNLOAD_DIR}")
        file(MAKE_DIRECTORY "${ORT_DOWNLOAD_DIR}")
    endif()

    # Download the package
    if(NOT EXISTS "${ORT_DOWNLOAD_PATH}")
        message(STATUS "Downloading ONNX Runtime v${ORT_VERSION} (${ORT_ARCH}) from ${ORT_DOWNLOAD_URL}...")
        file(DOWNLOAD "${ORT_DOWNLOAD_URL}" "${ORT_DOWNLOAD_PATH}" SHOW_PROGRESS STATUS DOWNLOAD_STATUS TIMEOUT 600)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ONNX Runtime. Status code: ${STATUS_CODE}")
        endif()
    else()
        message(STATUS "Using already downloaded archive at ${ORT_DOWNLOAD_PATH}")
    endif()

    # Extract the package
    if(NOT EXISTS "${ORT_EXTRACT_DIR}")
        message(STATUS "Extracting ${ORT_DOWNLOAD_PATH} to ${ORT_DOWNLOAD_DIR}...") # Extract to parent first
        if(WIN32)
            execute_process(
                COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '${ORT_DOWNLOAD_PATH}' -DestinationPath '${ORT_DOWNLOAD_DIR}' -Force"
                RESULT_VARIABLE EXTRACT_RESULT
                OUTPUT_QUIET ERROR_QUIET
            )
        elseif(UNIX)
            # Assumes 'tar' command is available
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${ORT_DOWNLOAD_PATH}"
                WORKING_DIRECTORY "${ORT_DOWNLOAD_DIR}"
                RESULT_VARIABLE EXTRACT_RESULT
                OUTPUT_QUIET ERROR_QUIET
            )
        else()
             message(FATAL_ERROR "Extraction logic not implemented for this OS.")
        endif()

        if(NOT EXTRACT_RESULT EQUAL 0)
            # Clean up potentially partial extraction
            file(REMOVE_RECURSE "${ORT_EXTRACT_DIR}") # Remove the target dir if extraction failed
            message(FATAL_ERROR "Failed to extract ONNX Runtime package. Result: ${EXTRACT_RESULT}. Archive: ${ORT_DOWNLOAD_PATH}")
        endif()

        # The extracted folder might have a different name (e.g. includes version/arch)
        # We rely on ORT_ROOT_GUESS being the correct final directory name
        if(NOT EXISTS "${ORT_EXTRACT_DIR}")
             # Sometimes the extraction doesn't create the top-level folder we expect
             # This is a heuristic - might need adjustment based on actual archive structure
             set(EXPECTED_EXTRACTED_FOLDER_NAME "onnxruntime-${ORT_ARCH}-${ORT_VERSION}") # Match package name part
             if(EXISTS "${ORT_DOWNLOAD_DIR}/${EXPECTED_EXTRACTED_FOLDER_NAME}")
                 message(STATUS "Renaming extracted folder ${EXPECTED_EXTRACTED_FOLDER_NAME} to ${ORT_PACKAGE_NAME}")
                 file(RENAME "${ORT_DOWNLOAD_DIR}/${EXPECTED_EXTRACTED_FOLDER_NAME}" "${ORT_EXTRACT_DIR}")
             else()
                 message(WARNING "Extraction complete, but expected directory '${ORT_EXTRACT_DIR}' not found directly. Manual check might be needed.")
                 # Attempt to find the actual extracted directory (simple check)
                 file(GLOB _extracted_dirs LIST_DIRECTORIES true RELATIVE "${ORT_DOWNLOAD_DIR}" "${ORT_DOWNLOAD_DIR}/onnxruntime-*")
                 if(_extracted_dirs)
                    list(GET _extracted_dirs 0 _found_dir_rel)
                    set(_found_dir_abs "${ORT_DOWNLOAD_DIR}/${_found_dir_rel}")
                    message(STATUS "Found potential extracted directory: ${_found_dir_abs}. Assuming this is the correct one.")
                    set(ORT_EXTRACT_DIR ${_found_dir_abs}) # Update the extract dir path
                 else()
                     message(FATAL_ERROR "Cannot determine ONNX Runtime extraction directory.")
                 endif()
             endif()
        endif()

        message(STATUS "ONNX Runtime extracted successfully to ${ORT_EXTRACT_DIR}.")
        # Delete archive after extraction
        message(STATUS "Removing archive ${ORT_DOWNLOAD_PATH}")
        file(REMOVE "${ORT_DOWNLOAD_PATH}")

    else()
         message(STATUS "ONNX Runtime already extracted at ${ORT_EXTRACT_DIR}")
    endif()

    # Set the output variable in the parent scope
    set(${output_ort_root_var} "${ORT_EXTRACT_DIR}" PARENT_SCOPE)
    message(STATUS "ONNX Runtime location set to ${ORT_EXTRACT_DIR}")
endfunction()

# Check if ONNX Runtime is available, download if not
if(NOT DEFINED ENV{ONNXRUNTIME_ROOT} AND NOT IS_DIRECTORY "${ORT_ROOT_GUESS}")
     message(STATUS "ONNX Runtime not found at ${ORT_ROOT_GUESS} or via ENV{ONNXRUNTIME_ROOT}. Attempting to download...")
     download_onnxruntime(ONNXRUNTIME_ROOT_DOWNLOADED) # Pass variable name to set
     set(ORT_ROOT ${ONNXRUNTIME_ROOT_DOWNLOADED}) # Use the downloaded path
elseif(DEFINED ENV{ONNXRUNTIME_ROOT})
     set(ORT_ROOT $ENV{ONNXRUNTIME_ROOT})
     message(STATUS "Using ONNX Runtime from ENV{ONNXRUNTIME_ROOT}: ${ORT_ROOT}")
else()
     set(ORT_ROOT ${ORT_ROOT_GUESS}) # Use the guessed path if it exists
     message(STATUS "Using existing ONNX Runtime at: ${ORT_ROOT}")
endif()

# Verify the final ORT_ROOT
if(NOT IS_DIRECTORY "${ORT_ROOT}")
    message(FATAL_ERROR "ONNX Runtime root directory not found or invalid after search/download attempt: ${ORT_ROOT}")
endif()

# Find ONNX Runtime include and library files using the determined ORT_ROOT
find_path(ONNXRUNTIME_INCLUDE_DIR NAMES onnxruntime_cxx_api.h PATHS "${ORT_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime PATHS "${ORT_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)

# Find ONNX Runtime include and library files using the determined ORT_ROOT
find_path(ONNXRUNTIME_INCLUDE_DIR NAMES onnxruntime_cxx_api.h PATHS "${ORT_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime PATHS "${ORT_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)

# ONNXRUNTIME_INCLUDE_DIR and ONNXRUNTIME_LIBRARY are now guaranteed to be found or CMake errors out.
message(STATUS "Found ONNX Runtime Include Dir: ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "Found ONNX Runtime Library: ${ONNXRUNTIME_LIBRARY}")


# --- Function to copy ONNX Runtime Shared Libraries (DLLs/SOs/DyLibs) ---
function(copy_ort_shared_libs target_name)
    message(STATUS "Locating ONNX Runtime shared libraries for target ${target_name}...")

    # Determine expected library extension and search paths
    if(CMAKE_HOST_WIN32)
        set(SHARED_LIB_EXT "dll")
        # Check both bin and lib as potential locations for DLLs
        set(SEARCH_DIRS "${ORT_ROOT}/bin" "${ORT_ROOT}/lib")
    elseif(CMAKE_HOST_APPLE)
        set(SHARED_LIB_EXT "dylib")
        set(SEARCH_DIRS "${ORT_ROOT}/lib") # Primary location on macOS
    elseif(CMAKE_HOST_UNIX)
        set(SHARED_LIB_EXT "so")
        set(SEARCH_DIRS "${ORT_ROOT}/lib") # Primary location on Linux
    else()
        message(WARNING "Shared library copying not fully implemented for this host OS.")
        return() # Exit function if OS not handled
    endif()

    set(FOUND_LIBS FALSE)
    set(ORT_SHARED_LIBS "") # Initialize list

    # Search in standard directories first
    foreach(DIR ${SEARCH_DIRS})
        if(EXISTS "${DIR}")
            # Use LIST_DIRECTORIES false to ensure we only get files
            file(GLOB POTENTIAL_LIBS LIST_DIRECTORIES false RELATIVE "${DIR}" "${DIR}/*.${SHARED_LIB_EXT}*")
            if(POTENTIAL_LIBS)
                foreach(LIB_REL_PATH ${POTENTIAL_LIBS})
                    # Construct absolute path
                    list(APPEND ORT_SHARED_LIBS "${DIR}/${LIB_REL_PATH}")
                endforeach()
                if(ORT_SHARED_LIBS) # Check if list is not empty after appending
                   set(FOUND_LIBS TRUE)
                   message(STATUS "Found shared libraries in: ${DIR}")
                endif()
                # Don't break, might be split between bin/lib etc.
            endif()
        endif()
    endforeach()

    # Recursive search as fallback if nothing found in standard locations
    if(NOT FOUND_LIBS)
        message(STATUS "No shared libs found in standard locations (${SEARCH_DIRS}), trying recursive search in ${ORT_ROOT}...")
        file(GLOB_RECURSE POTENTIAL_LIBS CONFIGURE_DEPENDS "${ORT_ROOT}/*.${SHARED_LIB_EXT}*")
        if(POTENTIAL_LIBS)
            set(ORT_SHARED_LIBS ${POTENTIAL_LIBS})
            set(FOUND_LIBS TRUE)
            message(STATUS "Found shared libs using recursive search.")
        else()
             message(WARNING "Recursive search also failed to find ONNX Runtime shared libraries (*.${SHARED_LIB_EXT}).")
        endif()
    endif()

    if(FOUND_LIBS)
        message(STATUS "Found ONNX Runtime shared libraries:")
        # Get the import library filename once
        get_filename_component(ORT_IMPORT_LIB_FILENAME ${ONNXRUNTIME_LIBRARY} NAME)

        foreach(ORT_LIB_PATH ${ORT_SHARED_LIBS})
            get_filename_component(LIB_NAME ${ORT_LIB_PATH} NAME)

            # Simple check to avoid copying the static/import library if it has the same name
            # as a shared library (common on Windows where onnxruntime.dll and onnxruntime.lib exist)
            # Only skip if it's the import library *and* we are on Windows
            if(NOT (CMAKE_HOST_WIN32 AND LIB_NAME STREQUAL ORT_IMPORT_LIB_FILENAME))
                 message(STATUS "  - ${LIB_NAME} (from ${ORT_LIB_PATH})")
                 # Use the generator expression directly in the command.
                 # It resolves to the correct directory at build time.
                 # No trailing slash needed for cmake -E copy_if_different <file> <dir>
                 add_custom_command(TARGET ${target_name} POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ORT_LIB_PATH}" "$<TARGET_FILE_DIR:${target_name}>"
                     COMMENT "Copying ${LIB_NAME} for target ${target_name}"
                     VERBATIM # Ensures arguments are passed correctly, especially paths
                 )
            else()
                 message(STATUS "  - Skipping copy of potential import library file: ${LIB_NAME}")
            endif()
        endforeach()
    else()
        message(WARNING "Could not find ONNX Runtime shared libraries (*.${SHARED_LIB_EXT}). Runtime loading might fail. Manual copy may be needed.")
    endif()
endfunction()


# --- Source Files ---
# Define core library files (used by both executable and module)
# IMPORTANT: Exclude main.cpp and bindings.cpp from this list
file(GLOB CORE_SOURCE_FILES CONFIGURE_DEPENDS
    "src/*.cpp"
    # Add other core source directories if needed: "src/core/*.cpp"
    EXCLUDE "src/main.cpp"      # Exclude the executable entry point
    EXCLUDE "src/bindings.cpp"  # Exclude the Python bindings entry point
)

# Define source file for the executable's entry point
set(EXECUTABLE_MAIN_SRC "src/main.cpp")
# Define source file for the Python module's entry point
set(PYTHON_BINDINGS_SRC "src/bindings.cpp")

# Verify core files exist
if(NOT CORE_SOURCE_FILES)
    message(WARNING "No core source files found in src/ (excluding main.cpp and bindings.cpp). Check GLOB pattern.")
    # Consider if this should be FATAL_ERROR depending on project structure
endif()
message(STATUS "Core source files: ${CORE_SOURCE_FILES}")

# --- Target Definitions ---

# Define common compiler flags
set(COMMON_COMPILE_FLAGS_DEBUG "")
set(COMMON_COMPILE_FLAGS_RELEASE "")
if(MSVC)
    list(APPEND COMMON_COMPILE_FLAGS_DEBUG /W4 /WX /Od /MP) # Debug: Warnings, warn-as-error, no optimization, parallel build
    list(APPEND COMMON_COMPILE_FLAGS_RELEASE /W4 /WX /O2 /MP) # Release: Warnings, warn-as-error, optimize speed, parallel build
else()
    list(APPEND COMMON_COMPILE_FLAGS_DEBUG -Wall -Wextra -Wpedantic -g -O0) # Debug: Warnings, debug symbols, no optimization
    list(APPEND COMMON_COMPILE_FLAGS_RELEASE -Wall -Wextra -Wpedantic -O3 -DNDEBUG) # Release: Warnings, optimize speed, define NDEBUG
endif()

# == Executable Target ==
if(BUILD_EXECUTABLE)
    set(EXECUTABLE_NAME "backtest_executable") # Renamed slightly to avoid conflict if names were same
    message(STATUS "--- Configuring Executable Target: ${EXECUTABLE_NAME} ---")

    # Check if main entry point exists
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${EXECUTABLE_MAIN_SRC}")
        message(FATAL_ERROR "Executable entry point '${EXECUTABLE_MAIN_SRC}' not found. Create it or set BUILD_EXECUTABLE=OFF.")
    endif()

    set(EXECUTABLE_SOURCES ${EXECUTABLE_MAIN_SRC} ${CORE_SOURCE_FILES})
    message(STATUS "Executable sources: ${EXECUTABLE_SOURCES}")

    add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SOURCES})

    # Include Directories for Executable
    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include # Project includes
        ${ONNXRUNTIME_INCLUDE_DIR}         # ONNX Runtime headers
    )

    # Link Libraries for Executable
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        ${ONNXRUNTIME_LIBRARY}
    )

    # Compiler Flags for Executable
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        $<$<CONFIG:Debug>:${COMMON_COMPILE_FLAGS_DEBUG}>
        $<$<CONFIG:Release>:${COMMON_COMPILE_FLAGS_RELEASE}>
        # Add other config types like RelWithDebInfo if needed
    )

    # Copy ONNX Runtime Shared Libraries for Executable
    copy_ort_shared_libs(${EXECUTABLE_NAME})

    # Set output directory for the executable
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Put executable in build/bin

    message(STATUS "Executable target ${EXECUTABLE_NAME} configured.")

endif()


# == Python Module Target ==
if(BUILD_PYTHON_MODULE)
    set(MODULE_NAME "cppbacktester_py") # Name for 'import cppbacktester_py'
    message(STATUS "--- Configuring Python Module Target: ${MODULE_NAME} ---")

    # Check if bindings entry point exists
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PYTHON_BINDINGS_SRC}")
        message(FATAL_ERROR "Python bindings entry point '${PYTHON_BINDINGS_SRC}' not found. Create it or set BUILD_PYTHON_MODULE=OFF.")
    endif()

    set(MODULE_SOURCES ${PYTHON_BINDINGS_SRC} ${CORE_SOURCE_FILES})
    message(STATUS "Module sources: ${MODULE_SOURCES}")

    pybind11_add_module(${MODULE_NAME} ${MODULE_SOURCES})

    # Include Directories for Module
    # pybind11 handles its own includes
    target_include_directories(${MODULE_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include # Project includes
        ${ONNXRUNTIME_INCLUDE_DIR}         # ONNX Runtime headers
    )

    # Link Libraries for Module
    # pybind11 handles its own linkage
    target_link_libraries(${MODULE_NAME} PRIVATE
        ${ONNXRUNTIME_LIBRARY}
    )

    # Compiler Flags for Module
    target_compile_options(${MODULE_NAME} PRIVATE
        $<$<CONFIG:Debug>:${COMMON_COMPILE_FLAGS_DEBUG}>
        $<$<CONFIG:Release>:${COMMON_COMPILE_FLAGS_RELEASE}>
    )

    # Copy ONNX Runtime Shared Libraries for Module
    copy_ort_shared_libs(${MODULE_NAME})

    # Set output directory for the Python module (e.g., next to executable or in a dedicated lib dir)
    # pybind11 often places it correctly relative to build dir for direct import/setup.py use.
    # Explicitly setting it can help consistency.
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # Put module in build/lib

    message(STATUS "Python module target ${MODULE_NAME} configured.")

endif()


# --- Installation (Optional) ---
# Example: Install executable and Python module
# You might need find_package(Python REQUIRED COMPONENTS Interpreter Development) for PYTHON_INSTALL_DIR
# include(GNUInstallDirs) # Provides standard install paths like CMAKE_INSTALL_BINDIR etc.
# if(BUILD_EXECUTABLE)
#     install(TARGETS ${EXECUTABLE_NAME}
#             RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # bin/
#     )
# endif()
# if(BUILD_PYTHON_MODULE)
#     install(TARGETS ${MODULE_NAME}
#             LIBRARY DESTINATION ${CMAKE_INSTALL_PYTHON_LIBRARIES_DIR} # Standard site-packages or dist-packages
#     )
# endif()


# --- Final Messages ---
message(STATUS "-------------------------------------------------------")
message(STATUS "CppBacktester Configuration Summary:")
if(BUILD_EXECUTABLE)
    message(STATUS "  Executable Target: ${EXECUTABLE_NAME}")
    message(STATUS "    Output: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXECUTABLE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
else()
    message(STATUS "  Executable Target: DISABLED (BUILD_EXECUTABLE=OFF)")
endif()
if(BUILD_PYTHON_MODULE)
    message(STATUS "  Python Module Target: ${MODULE_NAME}")
    message(STATUS "    Output: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    message(STATUS "    pybind11: Enabled")
else()
    message(STATUS "  Python Module Target: DISABLED (BUILD_PYTHON_MODULE=OFF)")
    message(STATUS "    pybind11: Skipped")
endif()
message(STATUS "  ONNX Runtime Root: ${ORT_ROOT}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE} (Use -DCMAKE_BUILD_TYPE=Release or Debug)")
message(STATUS "Configuration complete.")
message(STATUS "To build, run: cmake --build . --config <Release|Debug>")
message(STATUS "Make sure ONNX Runtime shared libs are available at runtime (copied to output dir or in PATH/LD_LIBRARY_PATH).")
message(STATUS "-------------------------------------------------------")