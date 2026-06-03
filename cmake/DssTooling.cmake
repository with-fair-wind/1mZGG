include_guard(GLOBAL)

if(NOT DSS_IS_MULTI_CONFIG)
    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/compile_commands.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
        DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
        COMMENT "Mirroring compile_commands.json into source root for clangd"
        VERBATIM
    )

    add_custom_target(dss_sync_compile_commands ALL
        DEPENDS "${CMAKE_SOURCE_DIR}/compile_commands.json"
    )
endif()

find_program(DSS_CLANG_FORMAT_EXECUTABLE
    NAMES clang-format-22 clang-format-21 clang-format-20 clang-format-19 clang-format-18 clang-format
    DOC "clang-format binary used by the format targets"
)

if(DSS_CLANG_FORMAT_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    set(DSS_RUN_CLANG_FORMAT "${CMAKE_SOURCE_DIR}/cmake/RunClangFormat.cmake")

    add_custom_target(format
        COMMAND ${CMAKE_COMMAND}
            "-DCLANG_FORMAT=${DSS_CLANG_FORMAT_EXECUTABLE}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DMODE=fix"
            -P "${DSS_RUN_CLANG_FORMAT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Formatting tracked C++ sources with clang-format"
        VERBATIM USES_TERMINAL
    )

    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND}
            "-DCLANG_FORMAT=${DSS_CLANG_FORMAT_EXECUTABLE}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DMODE=check"
            -P "${DSS_RUN_CLANG_FORMAT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Checking tracked C++ source formatting"
        VERBATIM USES_TERMINAL
    )
else()
    message(STATUS "clang-format not found or this is not a git checkout; format targets are disabled.")
endif()

find_program(DSS_CLANG_TIDY_EXECUTABLE
    NAMES clang-tidy-22 clang-tidy-21 clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy
    DOC "clang-tidy binary used by the tidy targets"
)

if(DSS_CLANG_TIDY_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/.git" AND NOT DSS_IS_MULTI_CONFIG)
    set(DSS_RUN_CLANG_TIDY "${CMAKE_SOURCE_DIR}/cmake/RunClangTidy.cmake")

    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND}
            "-DCLANG_TIDY=${DSS_CLANG_TIDY_EXECUTABLE}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DMODE=fix"
            "-DWARNINGS_AS_ERRORS=OFF"
            -P "${DSS_RUN_CLANG_TIDY}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running clang-tidy --fix over project sources"
        VERBATIM USES_TERMINAL
    )

    add_custom_target(tidy-check
        COMMAND ${CMAKE_COMMAND}
            "-DCLANG_TIDY=${DSS_CLANG_TIDY_EXECUTABLE}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DMODE=check"
            "-DWARNINGS_AS_ERRORS=${DSS_CLANG_TIDY_WARNINGS_AS_ERRORS}"
            -P "${DSS_RUN_CLANG_TIDY}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running clang-tidy over project sources"
        VERBATIM USES_TERMINAL
    )
else()
    message(STATUS "clang-tidy not found, this is not a git checkout, or generator is multi-config; tidy targets are disabled.")
endif()
