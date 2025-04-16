# FindPybind11.cmake
# This module finds the pybind11 headers and sets up the necessary include paths

# Look for pybind11 in standard locations
find_path(PYBIND11_INCLUDE_DIR
    NAMES pybind11/pybind11.h
    PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11
    ${CMAKE_SOURCE_DIR}/third_party/pybind11/include
    ${CMAKE_SOURCE_DIR}/third_party/pybind11
    DOC "pybind11 include directory"
)

# Handle standard find_package arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pybind11 DEFAULT_MSG PYBIND11_INCLUDE_DIR)

# Set output variables
if(PYBIND11_FOUND)
    set(PYBIND11_INCLUDE_DIRS ${PYBIND11_INCLUDE_DIR})
    message(STATUS "Found pybind11: ${PYBIND11_INCLUDE_DIR}")
endif()

# Mark as advanced
mark_as_advanced(PYBIND11_INCLUDE_DIR)
