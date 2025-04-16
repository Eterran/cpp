function(download_onnxruntime output_ort_root_var ORT_VERSION ORT_ARCH ORT_DOWNLOAD_DIR)
    # Determine appropriate archive format
    if(WIN32)
        set(ORT_ARCHIVE_SUFFIX "zip")
    else()
        set(ORT_ARCHIVE_SUFFIX "tgz")
    endif()

    set(ORT_PACKAGE_NAME "onnxruntime-${ORT_ARCH}-${ORT_VERSION}")
    set(ORT_DOWNLOAD_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/${ORT_PACKAGE_NAME}.${ORT_ARCHIVE_SUFFIX}")
    set(ORT_DOWNLOAD_PATH "${ORT_DOWNLOAD_DIR}/${ORT_PACKAGE_NAME}.${ORT_ARCHIVE_SUFFIX}")
    set(ORT_EXTRACT_DIR "${ORT_DOWNLOAD_DIR}/${ORT_PACKAGE_NAME}")

    # Create download directory if needed
    if(NOT EXISTS "${ORT_DOWNLOAD_DIR}")
        file(MAKE_DIRECTORY "${ORT_DOWNLOAD_DIR}")
    endif()

    # Download the package if not already present
    if(NOT EXISTS "${ORT_DOWNLOAD_PATH}")
        message(STATUS "Downloading ONNX Runtime v${ORT_VERSION} (${ORT_ARCH})...")
        file(DOWNLOAD "${ORT_DOWNLOAD_URL}" "${ORT_DOWNLOAD_PATH}" SHOW_PROGRESS
             STATUS DOWNLOAD_STATUS TIMEOUT 600)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ONNX Runtime. Status code: ${STATUS_CODE}")
        endif()
    else()
        message(STATUS "Using already downloaded archive at ${ORT_DOWNLOAD_PATH}")
    endif()

    # Extract the package if needed
    if(NOT EXISTS "${ORT_EXTRACT_DIR}")
        message(STATUS "Extracting ONNX Runtime archive...")
        if(WIN32)
            execute_process(
                COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
                        "Expand-Archive -Path '${ORT_DOWNLOAD_PATH}' -DestinationPath '${ORT_DOWNLOAD_DIR}' -Force"
                RESULT_VARIABLE EXTRACT_RESULT
            )
        elseif(UNIX)
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${ORT_DOWNLOAD_PATH}"
                WORKING_DIRECTORY "${ORT_DOWNLOAD_DIR}"
                RESULT_VARIABLE EXTRACT_RESULT
            )
        else()
            message(FATAL_ERROR "Extraction logic not implemented for this OS.")
        endif()

        if(NOT EXTRACT_RESULT EQUAL 0)
            file(REMOVE_RECURSE "${ORT_EXTRACT_DIR}")
            message(FATAL_ERROR "Failed to extract ONNX Runtime package.")
        endif()

        # Handle potential naming discrepancies
        if(NOT EXISTS "${ORT_EXTRACT_DIR}")
            file(GLOB _extracted_dirs LIST_DIRECTORIES true RELATIVE "${ORT_DOWNLOAD_DIR}"
                 "${ORT_DOWNLOAD_DIR}/onnxruntime-*")
            if(_extracted_dirs)
                list(GET _extracted_dirs 0 _found_dir_rel)
                set(_found_dir_abs "${ORT_DOWNLOAD_DIR}/${_found_dir_rel}")
                message(STATUS "Found extracted directory: ${_found_dir_abs}")
                set(ORT_EXTRACT_DIR "${_found_dir_abs}")
            else()
                message(FATAL_ERROR "Cannot determine ONNX Runtime extraction directory.")
            endif()
        endif()

        # Delete archive after successful extraction
        file(REMOVE "${ORT_DOWNLOAD_PATH}")
    else()
        message(STATUS "ONNX Runtime already extracted at ${ORT_EXTRACT_DIR}")
    endif()

    # Return the ORT root path
    set(${output_ort_root_var} "${ORT_EXTRACT_DIR}" PARENT_SCOPE)
endfunction()