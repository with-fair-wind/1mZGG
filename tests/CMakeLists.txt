find_package(GTest REQUIRED)

include(GoogleTest)

function(dss_add_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${TEST_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        Qt6::Core
    )
    target_precompile_headers(${TEST_NAME} REUSE_FROM DSS_QT)
    gtest_discover_tests(${TEST_NAME})
endfunction()

# Tests will be added as modules are implemented:
# dss_add_test(test_labeler     test_labeler.cpp)
# dss_add_test(test_track_algo  test_track_algo.cpp)
# dss_add_test(test_frame_codec test_frame_codec.cpp)
# dss_add_test(test_math_utils  test_math_utils.cpp)
