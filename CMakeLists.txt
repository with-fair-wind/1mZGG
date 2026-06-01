cmake_minimum_required(VERSION 3.28)

project(DSS_QT
    VERSION 2.0.0
    LANGUAGES CXX
    DESCRIPTION "Astronomical High Frame-Rate Image Processing System"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DSS_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(DSS_ARCHIVE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib")
set(DSS_LIBRARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DSS_RUNTIME_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${DSS_ARCHIVE_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${DSS_LIBRARY_OUTPUT_DIR}")

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${DSS_RUNTIME_OUTPUT_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config} "${DSS_ARCHIVE_OUTPUT_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} "${DSS_LIBRARY_OUTPUT_DIR}")
endforeach()

get_property(DSS_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
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

option(DSS_BUILD_APP       "Build the Qt desktop application"       ON)
option(DSS_ENABLE_TESTS    "Build unit tests"                       ON)
option(DSS_ENABLE_CUDA     "Build CUDA GPU backend and kernels"     OFF)
option(DSS_ENABLE_OPENCV   "Build OpenCV processing backend"        ON)
option(DSS_ENABLE_SAPERA   "Build with Sapera SDK support"          OFF)
option(DSS_ENABLE_STARLIBS "Build with StarMap/Photometry support"  OFF)
option(DSS_CLANG_TIDY_WARNINGS_AS_ERRORS "Treat clang-tidy diagnostics as errors in tidy-check" OFF)

if(WIN32)
    option(DSS_ENABLE_QT_DEPLOY "Run windeployqt after building the Qt application" ON)
else()
    option(DSS_ENABLE_QT_DEPLOY "Run windeployqt after building the Qt application" OFF)
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

add_compile_definitions(QT_NO_EMIT)

function(dss_target_defaults target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_23)
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4 /permissive- /Zc:__cplusplus /utf-8
            /wd4100
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wno-unused-parameter
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wno-unused-parameter
        )
    endif()
endfunction()

function(dss_std_pch target_name)
    target_precompile_headers(${target_name} PRIVATE
        <algorithm>
        <array>
        <atomic>
        <cassert>
        <cmath>
        <condition_variable>
        <cstdint>
        <expected>
        <filesystem>
        <format>
        <functional>
        <memory>
        <mutex>
        <optional>
        <ranges>
        <shared_mutex>
        <span>
        <string>
        <string_view>
        <thread>
        <vector>
    )
endfunction()

function(dss_qt_pch target_name)
    dss_std_pch(${target_name})
    target_precompile_headers(${target_name} PRIVATE
        <QObject>
        <QDebug>
        <QString>
    )
endfunction()

function(dss_imported_location target_name output_var)
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL "")
        if(config)
            get_target_property(location ${target_name} IMPORTED_LOCATION_${config})
        else()
            get_target_property(location ${target_name} IMPORTED_LOCATION)
        endif()

        if(location)
            set(${output_var} "${location}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${output_var} "" PARENT_SCOPE)
endfunction()

function(dss_enable_qt_deploy target_name)
    if(NOT WIN32 OR NOT DSS_ENABLE_QT_DEPLOY)
        return()
    endif()

    set(qt_bin_hints)
    if(Qt6_DIR)
        get_filename_component(qt_root "${Qt6_DIR}/../../.." ABSOLUTE)
        list(APPEND qt_bin_hints "${qt_root}/bin")
    endif()
    if(Qt6Core_DIR)
        get_filename_component(qt_core_root "${Qt6Core_DIR}/../../.." ABSOLUTE)
        list(APPEND qt_bin_hints "${qt_core_root}/bin")
    endif()

    find_program(DSS_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS ${qt_bin_hints}
        DOC "Qt windeployqt executable used to deploy the DSS_QT runtime"
    )

    if(DSS_WINDEPLOYQT_EXECUTABLE)
        set(deploy_tool "${DSS_WINDEPLOYQT_EXECUTABLE}")
    elseif(TARGET Qt6::windeployqt)
        set(deploy_tool "$<TARGET_FILE:Qt6::windeployqt>")
    else()
        message(WARNING "windeployqt not found; Qt runtime deployment for ${target_name} is disabled.")
        return()
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND "${deploy_tool}"
            --dir "$<TARGET_FILE_DIR:${target_name}>"
            "$<TARGET_FILE:${target_name}>"
        COMMENT "Deploying Qt runtime with windeployqt into $<TARGET_FILE_DIR:${target_name}>"
        VERBATIM
    )
endfunction()

find_package(spdlog QUIET)
find_package(nlohmann_json QUIET)

if(NOT TARGET nlohmann_json::nlohmann_json)
    message(FATAL_ERROR "nlohmann_json not found. Run conan install before configuring this project.")
endif()

if(DSS_ENABLE_TESTS)
    find_package(GTest QUIET)
    if(NOT TARGET GTest::gtest_main)
        message(WARNING "GTest not found; unit tests are disabled. Use Conan or provide GTestConfig.cmake.")
        set(DSS_ENABLE_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
    elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        dss_imported_location(GTest::gtest DSS_GTEST_LOCATION)
        if(DSS_GTEST_LOCATION MATCHES "\\.lib$")
            message(WARNING "MSVC GTest library found while using GNU; unit tests are disabled to avoid ABI mismatch.")
            set(DSS_ENABLE_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
        endif()
    endif()
endif()

if(DSS_ENABLE_OPENCV)
    find_package(OpenCV QUIET COMPONENTS core imgproc)
    if(OpenCV_FOUND)
        set(DSS_HAS_OPENCV ON)
    else()
        message(WARNING "OpenCV not found; OpenCV processing backend is disabled.")
        set(DSS_HAS_OPENCV OFF)
    endif()
else()
    set(DSS_HAS_OPENCV OFF)
endif()

if(DSS_ENABLE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        set(CMAKE_CUDA_STANDARD 20)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)
        find_package(CUDAToolkit REQUIRED)
        set(DSS_HAS_CUDA ON)
    else()
        message(WARNING "CUDA compiler not found; CUDA backend is disabled.")
        set(DSS_HAS_CUDA OFF)
    endif()
else()
    set(DSS_HAS_CUDA OFF)
endif()

if(DSS_BUILD_APP)
    if(WIN32 AND NOT DEFINED ENV{DSS_QT_ROOT} AND NOT CMAKE_PREFIX_PATH AND NOT Qt6_DIR)
        message(STATUS
            "DSS_QT_ROOT is not set. For local Qt presets, set it to your Qt root, "
            "for example F:/Qt/6.11.1/msvc2022_64."
        )
    endif()

    find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Network SerialPort Charts)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ElaWidgetTools/CMakeLists.txt)
            add_subdirectory(third_party/ElaWidgetTools)
            set(DSS_HAS_ELA ON)
        else()
            message(STATUS "ElaWidgetTools not found, building with Qt Widgets fallback.")
            set(DSS_HAS_ELA OFF)
        endif()
    else()
        message(WARNING
            "Qt6 not found; DSS_BUILD_APP is disabled for this configure. "
            "Set DSS_QT_ROOT, CMAKE_PREFIX_PATH, or Qt6_DIR to your Qt root "
            "(for example F:/Qt/6.11.1/msvc2022_64) to build the desktop app."
        )
        set(DSS_BUILD_APP OFF CACHE BOOL "Build the Qt desktop application" FORCE)
        set(DSS_HAS_ELA OFF)
    endif()
endif()

file(GLOB_RECURSE DSS_CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB_RECURSE DSS_APP_SOURCES CONFIGURE_DEPENDS src/app/*.cpp)
file(GLOB_RECURSE DSS_PROCESSING_SOURCES CONFIGURE_DEPENDS src/processing/*.cpp)
file(GLOB_RECURSE DSS_TRACKING_SOURCES CONFIGURE_DEPENDS src/tracking/*.cpp)
file(GLOB_RECURSE DSS_COMM_QT_SOURCES CONFIGURE_DEPENDS src/comm/*.cpp)
file(GLOB_RECURSE DSS_NETWORK_QT_SOURCES CONFIGURE_DEPENDS src/network/*.cpp)
file(GLOB_RECURSE DSS_UI_QT_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp)
file(GLOB_RECURSE DSS_UI_QT_HEADERS CONFIGURE_DEPENDS include/dss/ui/*.h)

list(FILTER DSS_PROCESSING_SOURCES EXCLUDE REGEX ".*/opencv_processing_strategy\\.cpp$")

add_library(dss_core STATIC ${DSS_CORE_SOURCES})
dss_target_defaults(dss_core)
dss_std_pch(dss_core)
target_link_libraries(dss_core PUBLIC nlohmann_json::nlohmann_json)

add_library(dss_app STATIC ${DSS_APP_SOURCES})
dss_target_defaults(dss_app)
dss_std_pch(dss_app)
target_link_libraries(dss_app PUBLIC dss_core)

add_library(dss_tracking STATIC ${DSS_TRACKING_SOURCES})
dss_target_defaults(dss_tracking)
dss_std_pch(dss_tracking)
target_link_libraries(dss_tracking PUBLIC dss_core)

add_library(dss_processing STATIC ${DSS_PROCESSING_SOURCES})
dss_target_defaults(dss_processing)
dss_std_pch(dss_processing)
target_link_libraries(dss_processing PUBLIC dss_core dss_tracking)

if(DSS_HAS_OPENCV)
    add_library(dss_processing_opencv STATIC src/processing/opencv_processing_strategy.cpp)
    dss_target_defaults(dss_processing_opencv)
    dss_std_pch(dss_processing_opencv)
    target_link_libraries(dss_processing_opencv
        PUBLIC
            dss_processing
            ${OpenCV_LIBS}
    )
    target_include_directories(dss_processing_opencv SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(dss_processing_opencv PUBLIC DSS_HAS_OPENCV=1)
endif()

if(DSS_HAS_CUDA)
    file(GLOB DSS_CUDA_SOURCES CONFIGURE_DEPENDS kernels/*.cu)
    add_library(dss_gpu_cuda STATIC
        src/gpu/cuda_device_manager.cpp
        ${DSS_CUDA_SOURCES}
    )
    dss_target_defaults(dss_gpu_cuda)
    target_link_libraries(dss_gpu_cuda PUBLIC dss_core CUDA::cudart)
    target_compile_definitions(dss_gpu_cuda PUBLIC DSS_HAS_CUDA=1)
    set_target_properties(dss_gpu_cuda PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES "50;60;70;75;80;86;89;90"
    )
endif()

if(DSS_BUILD_APP)
    add_library(dss_comm_qt STATIC ${DSS_COMM_QT_SOURCES})
    dss_target_defaults(dss_comm_qt)
    dss_qt_pch(dss_comm_qt)
    target_link_libraries(dss_comm_qt PUBLIC dss_core Qt6::Core Qt6::SerialPort)

    add_library(dss_network_qt STATIC ${DSS_NETWORK_QT_SOURCES})
    dss_target_defaults(dss_network_qt)
    dss_qt_pch(dss_network_qt)
    target_link_libraries(dss_network_qt PUBLIC dss_core Qt6::Core Qt6::Network)

    add_library(dss_ui_qt STATIC ${DSS_UI_QT_SOURCES} ${DSS_UI_QT_HEADERS})
    dss_target_defaults(dss_ui_qt)
    dss_qt_pch(dss_ui_qt)
    target_link_libraries(dss_ui_qt
        PUBLIC
            dss_core
            dss_processing
            dss_tracking
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Charts
    )

    if(DSS_HAS_ELA)
        target_compile_definitions(dss_ui_qt PUBLIC DSS_HAS_ELA=1)
        target_link_libraries(dss_ui_qt PUBLIC elawidgettools)
    endif()

    add_executable(DSS_QT src/main.cpp)
    dss_target_defaults(DSS_QT)
    dss_qt_pch(DSS_QT)
    target_compile_definitions(DSS_QT PRIVATE
        QT_DEPRECATED_WARNINGS
        $<$<BOOL:${DSS_ENABLE_SAPERA}>:DSS_HAS_SAPERA>
        $<$<BOOL:${DSS_ENABLE_STARLIBS}>:DSS_HAS_STARLIBS>
    )
    target_link_libraries(DSS_QT PRIVATE
        dss_core
        dss_app
        dss_processing
        dss_tracking
        dss_comm_qt
        dss_network_qt
        dss_ui_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::SerialPort
        Qt6::Charts
    )

    if(DSS_HAS_OPENCV)
        target_link_libraries(DSS_QT PRIVATE dss_processing_opencv)
    endif()

    if(DSS_HAS_CUDA)
        target_link_libraries(DSS_QT PRIVATE dss_gpu_cuda)
    endif()

    if(DSS_ENABLE_SAPERA)
        find_library(SAPERA_LIB Sapera++ PATHS /usr/local/lib)
        find_library(SAPERALT_LIB SaperaLT PATHS /usr/local/lib)
        find_library(CORW32_LIB CorW32 PATHS /usr/local/lib)
        if(SAPERA_LIB AND SAPERALT_LIB AND CORW32_LIB)
            target_link_libraries(DSS_QT PRIVATE ${SAPERA_LIB} ${SAPERALT_LIB} ${CORW32_LIB})
            target_include_directories(DSS_QT PRIVATE
                /usr/local/include/Sapera/include
                /usr/local/include/Sapera/classes/basic
            )
        else()
            message(WARNING "Sapera SDK not found, disabling hardware grabber.")
        endif()
    endif()

    if(DSS_ENABLE_STARLIBS)
        find_library(STARMAP_LIB StarMap PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../proLib)
        find_library(PHOTOMETRY_LIB dllPhotometry PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../proLib)
        find_library(STARPOSITION_LIB StarPosition PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../proLib)
        if(STARMAP_LIB AND PHOTOMETRY_LIB AND STARPOSITION_LIB)
            target_link_libraries(DSS_QT PRIVATE
                ${STARMAP_LIB}
                ${PHOTOMETRY_LIB}
                ${STARPOSITION_LIB}
            )
        else()
            message(WARNING "Star libraries not found.")
        endif()
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/dss.qrc)
        target_sources(DSS_QT PRIVATE resources/dss.qrc)
    endif()

    add_custom_command(TARGET DSS_QT POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "${CMAKE_BINARY_DIR}/config"
        COMMENT "Copying runtime JSON config into ${CMAKE_BINARY_DIR}/config"
        VERBATIM
    )

    dss_enable_qt_deploy(DSS_QT)
endif()

if(DSS_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
