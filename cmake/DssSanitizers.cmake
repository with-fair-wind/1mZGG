include_guard(GLOBAL)

set(DSS_SANITIZER "none" CACHE STRING "Runtime checker: none, address, thread")
set_property(CACHE DSS_SANITIZER PROPERTY STRINGS none address thread)
if(DSS_SANITIZER STREQUAL "none")
    return()
endif()
if(NOT DSS_SANITIZER MATCHES "^(address|thread)$")
    message(FATAL_ERROR "Unknown DSS_SANITIZER: ${DSS_SANITIZER}")
endif()
if(DSS_ENABLE_CUDA)
    message(FATAL_ERROR "DSS_SANITIZER requires DSS_ENABLE_CUDA=OFF; validate CUDA separately")
endif()
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU|AppleClang)$")
    message(FATAL_ERROR "DSS_SANITIZER currently supports Clang or GCC")
endif()
if(DSS_SANITIZER STREQUAL "thread" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "DSS_SANITIZER=thread is configured for Linux only")
endif()

if(MSVC)
    if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR
            "clang-cl ASan requires a single-config Release/RelWithDebInfo build and matching Release CRT dependencies")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM|arm|aarch)")
        message(FATAL_ERROR "The clang-cl ASan configuration currently supports Windows x64")
    endif()
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
        OUTPUT_VARIABLE DSS_CLANG_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    file(TO_CMAKE_PATH "${DSS_CLANG_RESOURCE_DIR}" DSS_CLANG_RESOURCE_DIR)
    # Scoop/compiler upgrades can remove the previously cached version directory.
    # Always resolve both libraries against the active compiler's resource tree.
    unset(DSS_ASAN_RUNTIME CACHE)
    unset(DSS_ASAN_THUNK CACHE)
    find_library(DSS_ASAN_RUNTIME NAMES clang_rt.asan_dynamic-x86_64
        PATHS "${DSS_CLANG_RESOURCE_DIR}/lib/windows" NO_DEFAULT_PATH REQUIRED)
    find_library(DSS_ASAN_THUNK NAMES clang_rt.asan_dynamic_runtime_thunk-x86_64
        PATHS "${DSS_CLANG_RESOURCE_DIR}/lib/windows" NO_DEFAULT_PATH REQUIRED)
    # clang-cl AddressSanitizer cannot coexist with MSVC runtime checks.
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        string(REGEX REPLACE "(^| )/RTC[^ ]*" "" CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
    endforeach()
endif()

function(dss_enable_sanitizer target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /fsanitize=address /Oy- /Zi)
        # CMake invokes lld-link directly, bypassing clang-cl's runtime injection.
        # Keep symbols and prevent folding instrumented sections in sanitizer builds.
        target_link_options(${target_name} PRIVATE /INCREMENTAL:NO /DEBUG /OPT:NOICF
            /INCLUDE:__asan_seh_interceptor "/WHOLEARCHIVE:${DSS_ASAN_THUNK}")
        target_link_libraries(${target_name} PRIVATE "${DSS_ASAN_RUNTIME}")
    else()
        target_compile_options(${target_name} PRIVATE -fsanitize=${DSS_SANITIZER} -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=${DSS_SANITIZER})
    endif()
endfunction()
