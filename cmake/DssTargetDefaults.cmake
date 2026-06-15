include_guard(GLOBAL)

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

    if(DSS_ENABLE_COLOR_DIAGNOSTICS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            if(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
                target_compile_options(${target_name} PRIVATE /clang:-fcolor-diagnostics)
            else()
                target_compile_options(${target_name} PRIVATE -fcolor-diagnostics)
            endif()
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target_name} PRIVATE -fdiagnostics-color=always)
        elseif(MSVC)
            target_compile_options(${target_name} PRIVATE /diagnostics:caret)
        endif()
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
