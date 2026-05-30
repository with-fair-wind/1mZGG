cmake_minimum_required(VERSION 3.28)
project(DSS_QT
    VERSION 2.0.0
    LANGUAGES CXX CUDA
    DESCRIPTION "Astronomical High Frame-Rate Image Processing System"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_compile_definitions(QT_NO_EMIT)

# --- Options ---
option(DSS_ENABLE_TESTS    "Build unit tests"                ON)
option(DSS_ENABLE_SAPERA   "Build with Sapera SDK support"   OFF)
option(DSS_ENABLE_STARLIBS "Build with StarMap/Photometry"   OFF)

# --- Conan-provided or system packages ---
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort Charts)
find_package(CUDAToolkit REQUIRED)

if(DSS_ENABLE_TESTS)
    find_package(GTest REQUIRED)
endif()

find_package(spdlog QUIET)
if(NOT TARGET spdlog::spdlog)
    message(STATUS "spdlog not found, logging will use fallback (qDebug)")
endif()

find_package(nlohmann_json QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    message(STATUS "nlohmann_json not found, JSON support disabled")
endif()

# --- Third-party submodules ---
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/ElaWidgetTools/CMakeLists.txt)
    add_subdirectory(third_party/ElaWidgetTools)
    set(DSS_HAS_ELA ON)
else()
    message(STATUS "ElaWidgetTools not found, building without Fluent UI")
    set(DSS_HAS_ELA OFF)
endif()

# --- CUDA Kernel Library ---
file(GLOB DSS_CUDA_SOURCES kernels/*.cu)
if(DSS_CUDA_SOURCES)
    add_library(dss_kernels STATIC ${DSS_CUDA_SOURCES})
    set_target_properties(dss_kernels PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES "50;60;70;75;80;86;89;90"
    )
    target_include_directories(dss_kernels PUBLIC include)
endif()

# --- Collect source files by module ---
file(GLOB_RECURSE DSS_CORE_SOURCES       src/core/*.cpp)
file(GLOB_RECURSE DSS_ACQUISITION_SOURCES src/acquisition/*.cpp)
file(GLOB_RECURSE DSS_PROCESSING_SOURCES  src/processing/*.cpp)
file(GLOB_RECURSE DSS_TRACKING_SOURCES    src/tracking/*.cpp)
file(GLOB_RECURSE DSS_GPU_SOURCES         src/gpu/*.cpp)
file(GLOB_RECURSE DSS_COMM_SOURCES        src/comm/*.cpp)
file(GLOB_RECURSE DSS_NETWORK_SOURCES     src/network/*.cpp)
file(GLOB_RECURSE DSS_STORAGE_SOURCES     src/storage/*.cpp)
file(GLOB_RECURSE DSS_UI_SOURCES          src/ui/*.cpp)

file(GLOB_RECURSE DSS_HEADERS             include/dss/*.h)

set(DSS_ALL_SOURCES
    src/main.cpp
    ${DSS_CORE_SOURCES}
    ${DSS_ACQUISITION_SOURCES}
    ${DSS_PROCESSING_SOURCES}
    ${DSS_TRACKING_SOURCES}
    ${DSS_GPU_SOURCES}
    ${DSS_COMM_SOURCES}
    ${DSS_NETWORK_SOURCES}
    ${DSS_STORAGE_SOURCES}
    ${DSS_UI_SOURCES}
    ${DSS_HEADERS}
)

# --- Main executable ---
add_executable(DSS_QT ${DSS_ALL_SOURCES})

target_include_directories(DSS_QT
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(DSS_QT PRIVATE
    QT_DEPRECATED_WARNINGS
    $<$<BOOL:${DSS_ENABLE_SAPERA}>:DSS_HAS_SAPERA>
    $<$<BOOL:${DSS_ENABLE_STARLIBS}>:DSS_HAS_STARLIBS>
)

# Compiler-specific flags
if(MSVC)
    target_compile_options(DSS_QT PRIVATE
        /W4 /permissive- /Zc:__cplusplus /utf-8
        /wd4100  # unreferenced formal parameter (during migration)
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(DSS_QT PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter  # during migration
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DSS_QT PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# --- Precompiled header ---
target_precompile_headers(DSS_QT PRIVATE
    <string>
    <string_view>
    <vector>
    <array>
    <span>
    <ranges>
    <memory>
    <functional>
    <optional>
    <expected>
    <thread>
    <mutex>
    <shared_mutex>
    <atomic>
    <condition_variable>
    <format>
    <cstdint>
    <cmath>
    <cassert>
    <QObject>
    <QString>
    <QDebug>
)

# --- Link libraries ---
target_link_libraries(DSS_QT PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::SerialPort
    Qt6::Charts
    CUDA::cudart
)

if(DSS_HAS_ELA)
    target_link_libraries(DSS_QT PRIVATE elawidgettools)
endif()

if(TARGET dss_kernels)
    target_link_libraries(DSS_QT PRIVATE dss_kernels)
endif()

if(TARGET spdlog::spdlog)
    target_link_libraries(DSS_QT PRIVATE spdlog::spdlog)
endif()

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(DSS_QT PRIVATE nlohmann_json::nlohmann_json)
endif()

# --- Optional: Sapera SDK ---
if(DSS_ENABLE_SAPERA)
    find_library(SAPERA_LIB   Sapera++  PATHS /usr/local/lib)
    find_library(SAPERALT_LIB SaperaLT  PATHS /usr/local/lib)
    find_library(CORW32_LIB   CorW32    PATHS /usr/local/lib)
    if(SAPERA_LIB AND SAPERALT_LIB AND CORW32_LIB)
        target_link_libraries(DSS_QT PRIVATE ${SAPERA_LIB} ${SAPERALT_LIB} ${CORW32_LIB})
        target_include_directories(DSS_QT PRIVATE
            /usr/local/include/Sapera/include
            /usr/local/include/Sapera/classes/basic
        )
    else()
        message(WARNING "Sapera SDK not found, disabling hardware grabber")
    endif()
endif()

# --- Optional: StarMap / Photometry ---
if(DSS_ENABLE_STARLIBS)
    find_library(STARMAP_LIB      StarMap       PATHS ${CMAKE_SOURCE_DIR}/../proLib)
    find_library(PHOTOMETRY_LIB   dllPhotometry PATHS ${CMAKE_SOURCE_DIR}/../proLib)
    find_library(STARPOSITION_LIB StarPosition  PATHS ${CMAKE_SOURCE_DIR}/../proLib)
    if(STARMAP_LIB AND PHOTOMETRY_LIB AND STARPOSITION_LIB)
        target_link_libraries(DSS_QT PRIVATE
            ${STARMAP_LIB} ${PHOTOMETRY_LIB} ${STARPOSITION_LIB}
        )
    else()
        message(WARNING "Star libraries not found")
    endif()
endif()

# --- Qt Resources ---
if(EXISTS ${CMAKE_SOURCE_DIR}/resources/dss.qrc)
    target_sources(DSS_QT PRIVATE resources/dss.qrc)
endif()

# --- Tests ---
if(DSS_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
