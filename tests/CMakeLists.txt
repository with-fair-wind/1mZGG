include(GoogleTest)

function(dss_add_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    dss_target_defaults(${TEST_NAME})
    target_link_libraries(${TEST_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        ${ARGN}
    )
    gtest_discover_tests(${TEST_NAME})
endfunction()

dss_add_test(test_frame_codec test_frame_codec.cpp dss_core)
dss_add_test(test_serial_protocol_codec test_serial_protocol_codec.cpp dss_core)
dss_add_test(test_config test_config.cpp dss_core)
dss_add_test(test_service_host test_service_host.cpp dss_core)
dss_add_test(test_service_registry test_service_registry.cpp dss_core)
dss_add_test(test_bounded_channel test_bounded_channel.cpp dss_processing)
dss_add_test(test_image_processor test_image_processor.cpp dss_processing)
dss_add_test(test_frame_view test_frame_view.cpp dss_processing)
dss_add_test(test_processing_pipeline test_processing_pipeline.cpp dss_processing)

if(TARGET dss_network_qt)
    dss_add_test(test_image_sender test_image_sender.cpp dss_network_qt)
endif()

if(TARGET dss_processing_opencv)
    dss_add_test(test_opencv_processing test_opencv_processing.cpp dss_processing_opencv)
endif()
