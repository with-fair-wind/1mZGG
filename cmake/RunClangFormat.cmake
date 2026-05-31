if(NOT CLANG_FORMAT)
    message(FATAL_ERROR "RunClangFormat.cmake: CLANG_FORMAT is not set")
endif()

if(NOT SOURCE_DIR)
    message(FATAL_ERROR "RunClangFormat.cmake: SOURCE_DIR is not set")
endif()

if(NOT MODE STREQUAL "fix" AND NOT MODE STREQUAL "check")
    message(FATAL_ERROR "RunClangFormat.cmake: MODE must be 'fix' or 'check'")
endif()

execute_process(
    COMMAND git -C "${SOURCE_DIR}" ls-files --
        "*.cpp" "*.cc" "*.cxx" "*.h" "*.hpp" "*.hxx" "*.cu" "*.cuh"
    OUTPUT_VARIABLE DSS_FORMAT_FILES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

if(DSS_FORMAT_FILES STREQUAL "")
    message(STATUS "No tracked C++ sources found; nothing to format.")
    return()
endif()

string(REPLACE "\n" ";" DSS_FORMAT_FILES "${DSS_FORMAT_FILES}")

set(DSS_FORMAT_ABS_FILES "")
foreach(file_path IN LISTS DSS_FORMAT_FILES)
    string(REPLACE "\\" "/" normalized_path "${file_path}")
    if(normalized_path MATCHES "^(oldsrc|third_party|build|\\.ref-repos)/")
        continue()
    endif()

    list(APPEND DSS_FORMAT_ABS_FILES "${SOURCE_DIR}/${file_path}")
endforeach()

if(NOT DSS_FORMAT_ABS_FILES)
    message(STATUS "No in-scope C++ sources found; nothing to format.")
    return()
endif()

if(MODE STREQUAL "fix")
    execute_process(
        COMMAND "${CLANG_FORMAT}" -i --style=file ${DSS_FORMAT_ABS_FILES}
        COMMAND_ERROR_IS_FATAL ANY
    )
else()
    execute_process(
        COMMAND "${CLANG_FORMAT}" --dry-run --Werror --style=file ${DSS_FORMAT_ABS_FILES}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
