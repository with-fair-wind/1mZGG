if(NOT CLANG_TIDY)
    message(FATAL_ERROR "RunClangTidy.cmake: CLANG_TIDY is not set")
endif()

if(NOT SOURCE_DIR)
    message(FATAL_ERROR "RunClangTidy.cmake: SOURCE_DIR is not set")
endif()

if(NOT BUILD_DIR)
    message(FATAL_ERROR "RunClangTidy.cmake: BUILD_DIR is not set")
endif()

if(NOT EXISTS "${BUILD_DIR}/compile_commands.json")
    message(FATAL_ERROR
        "RunClangTidy.cmake: ${BUILD_DIR}/compile_commands.json not found. Configure the preset first."
    )
endif()

if(NOT MODE STREQUAL "fix" AND NOT MODE STREQUAL "check")
    message(FATAL_ERROR "RunClangTidy.cmake: MODE must be 'fix' or 'check'")
endif()

set(DSS_TIDY_EXTRA_ARGS "")
if(MODE STREQUAL "fix")
    list(APPEND DSS_TIDY_EXTRA_ARGS -fix)
endif()

if(WARNINGS_AS_ERRORS)
    list(APPEND DSS_TIDY_EXTRA_ARGS -warnings-as-errors=*)
endif()

string(REPLACE "\\" "/" DSS_SOURCE_DIR_RE "${SOURCE_DIR}")
set(DSS_SOURCE_FILTER "${DSS_SOURCE_DIR_RE}/(src|tests)/.*")

if(NOT WIN32)
    get_filename_component(CLANG_TIDY_DIR "${CLANG_TIDY}" DIRECTORY)
    set(DSS_RUN_CLANG_TIDY_CANDIDATES
        "${CLANG_TIDY_DIR}/run-clang-tidy"
        "${CLANG_TIDY_DIR}/run-clang-tidy.py"
    )

    foreach(candidate IN LISTS DSS_RUN_CLANG_TIDY_CANDIDATES)
        if(EXISTS "${candidate}")
            include(ProcessorCount)
            ProcessorCount(DSS_TIDY_JOBS)
            if(DSS_TIDY_JOBS EQUAL 0)
                set(DSS_TIDY_JOBS 4)
            endif()

            execute_process(
                COMMAND "${candidate}"
                    -p "${BUILD_DIR}"
                    -clang-tidy-binary "${CLANG_TIDY}"
                    -j ${DSS_TIDY_JOBS}
                    -quiet
                    ${DSS_TIDY_EXTRA_ARGS}
                    "${DSS_SOURCE_FILTER}"
                COMMAND_ERROR_IS_FATAL ANY
            )
            return()
        endif()
    endforeach()
endif()

file(READ "${BUILD_DIR}/compile_commands.json" DSS_COMPILE_COMMANDS)
string(JSON DSS_COMPILE_COMMANDS_LENGTH ERROR_VARIABLE json_error LENGTH "${DSS_COMPILE_COMMANDS}")
if(json_error)
    message(FATAL_ERROR "RunClangTidy.cmake: failed to parse compile_commands.json: ${json_error}")
endif()

set(DSS_TIDY_FILES "")
if(DSS_COMPILE_COMMANDS_LENGTH GREATER 0)
    math(EXPR last_index "${DSS_COMPILE_COMMANDS_LENGTH} - 1")
    foreach(index RANGE 0 ${last_index})
        string(JSON compile_file GET "${DSS_COMPILE_COMMANDS}" ${index} file)
        if(NOT IS_ABSOLUTE "${compile_file}")
            string(JSON compile_directory GET "${DSS_COMPILE_COMMANDS}" ${index} directory)
            file(REAL_PATH "${compile_file}" compile_file BASE_DIRECTORY "${compile_directory}")
        endif()

        string(REPLACE "\\" "/" normalized_file "${compile_file}")
        if(normalized_file MATCHES "^${DSS_SOURCE_DIR_RE}/(src|tests)/.*\\.(cpp|cc|cxx)$")
            list(APPEND DSS_TIDY_FILES "${compile_file}")
        endif()
    endforeach()
endif()

list(REMOVE_DUPLICATES DSS_TIDY_FILES)

if(NOT DSS_TIDY_FILES)
    message(STATUS "No in-scope source files found in compile_commands.json; nothing to tidy.")
    return()
endif()

execute_process(
    COMMAND "${CLANG_TIDY}"
        -p "${BUILD_DIR}"
        ${DSS_TIDY_EXTRA_ARGS}
        ${DSS_TIDY_FILES}
    COMMAND_ERROR_IS_FATAL ANY
)
