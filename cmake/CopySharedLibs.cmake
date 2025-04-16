# Function to copy ONNX Runtime shared libraries to output directories
function(copy_ort_shared_libs target_name ort_root onnxruntime_lib output_dir)
    # Determine expected library extension
    if(CMAKE_HOST_WIN32)
        set(SHARED_LIB_EXT "dll")
        set(SEARCH_DIRS "${ort_root}/bin" "${ort_root}/lib")
    elseif(CMAKE_HOST_APPLE)
        set(SHARED_LIB_EXT "dylib")
        set(SEARCH_DIRS "${ort_root}/lib")
    elseif(CMAKE_HOST_UNIX)
        set(SHARED_LIB_EXT "so")
        set(SEARCH_DIRS "${ort_root}/lib")
    else()
        message(WARNING "Shared library copying not implemented for this OS.")
        return()
    endif()

    set(FOUND_LIBS FALSE)
    set(ORT_SHARED_LIBS "")

    # Search for libraries
    foreach(DIR ${SEARCH_DIRS})
        if(EXISTS "${DIR}")
            file(GLOB POTENTIAL_LIBS LIST_DIRECTORIES false
                 RELATIVE "${DIR}" "${DIR}/*.${SHARED_LIB_EXT}*")
            if(POTENTIAL_LIBS)
                foreach(LIB_REL_PATH ${POTENTIAL_LIBS})
                    list(APPEND ORT_SHARED_LIBS "${DIR}/${LIB_REL_PATH}")
                endforeach()
                set(FOUND_LIBS TRUE)
            endif()
        endif()
    endforeach()

    # Recursive search as fallback
    if(NOT FOUND_LIBS)
        message(STATUS "Trying recursive search for shared libraries...")
        file(GLOB_RECURSE POTENTIAL_LIBS "${ort_root}/*.${SHARED_LIB_EXT}*")
        if(POTENTIAL_LIBS)
            set(ORT_SHARED_LIBS ${POTENTIAL_LIBS})
            set(FOUND_LIBS TRUE)
        endif()
    endif()

    if(FOUND_LIBS)
        message(STATUS "Found ONNX Runtime shared libraries to copy")
        get_filename_component(ORT_IMPORT_LIB_FILENAME ${onnxruntime_lib} NAME)

        # Create directories for shared libraries if they don't exist
        set(SHARED_LIBS_DIR "${output_dir}/bin")
        file(MAKE_DIRECTORY ${SHARED_LIBS_DIR})

        # For Python module, we also need to copy DLLs to the module directory
        if("${target_name}" STREQUAL "cppbacktester_py")
            set(MODULE_DIR "${output_dir}/lib")
            file(MAKE_DIRECTORY ${MODULE_DIR})
        endif()

        # Copy all shared libraries to the appropriate directories
        foreach(ORT_LIB_PATH ${ORT_SHARED_LIBS})
            get_filename_component(LIB_NAME ${ORT_LIB_PATH} NAME)

            # Skip import library on Windows
            if(NOT (CMAKE_HOST_WIN32 AND LIB_NAME STREQUAL ORT_IMPORT_LIB_FILENAME))
                # Copy to the shared libs directory
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${ORT_LIB_PATH}" "${SHARED_LIBS_DIR}"
                    COMMENT "Copying ${LIB_NAME} to shared libs directory"
                    VERBATIM
                )

                # For Python module, also copy to the module directory
                if("${target_name}" STREQUAL "cppbacktester_py")
                    add_custom_command(TARGET ${target_name} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${ORT_LIB_PATH}" "${MODULE_DIR}"
                        COMMENT "Copying ${LIB_NAME} to Python module directory"
                        VERBATIM
                    )
                endif()
            endif()
        endforeach()
    else()
        message(WARNING "Could not find ONNX Runtime shared libraries. Manual copy may be needed.")
    endif()
endfunction()