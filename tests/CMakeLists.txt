include(GoogleTest)

function(dss_configure_test_target TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    dss_target_defaults(${TEST_NAME})
    target_link_libraries(${TEST_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        ${ARGN}
    )
endfunction()

function(dss_add_test TEST_NAME TEST_SOURCE)
    dss_configure_test_target(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
    gtest_discover_tests(${TEST_NAME})
endfunction()

function(dss_add_qt_test TEST_NAME TEST_SOURCE)
    dss_configure_test_target(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    if(WIN32 AND DSS_QT_RUNTIME_BIN_DIR)
        set_tests_properties(${TEST_NAME} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${DSS_QT_RUNTIME_BIN_DIR}"
        )
    endif()
endfunction()

dss_add_test(test_frame_codec test_frame_codec.cpp dss_core)
dss_add_test(test_serial_protocol_codec test_serial_protocol_codec.cpp dss_core)
dss_add_test(test_camera_control_protocol test_camera_control_protocol.cpp dss_core)
dss_add_test(test_image_storage_format test_image_storage_format.cpp dss_core)
dss_add_test(test_bmp_image_format test_bmp_image_format.cpp dss_core)
dss_add_test(test_track_data_storage_format test_track_data_storage_format.cpp dss_core)
dss_add_test(test_data_exchange_protocol test_data_exchange_protocol.cpp dss_core)
dss_add_test(test_network_protocols test_network_protocols.cpp dss_core)
dss_add_test(test_config test_config.cpp dss_core)
dss_add_test(test_service_host test_service_host.cpp dss_core)
dss_add_test(test_service_registry test_service_registry.cpp dss_core)
dss_add_test(test_math_utils test_math_utils.cpp dss_tracking)
dss_add_test(test_bounded_channel test_bounded_channel.cpp dss_processing)
dss_add_test(test_image_processor test_image_processor.cpp dss_processing)
dss_add_test(test_frame_view test_frame_view.cpp dss_processing)
dss_add_test(test_processing_pipeline test_processing_pipeline.cpp dss_processing)

if(TARGET dss_network_qt)
    dss_add_test(test_image_sender test_image_sender.cpp dss_network_qt)
    dss_add_test(test_heartbeat test_heartbeat.cpp dss_network_qt)
endif()

if(TARGET dss_comm_qt AND TARGET dss_network_qt)
    dss_add_qt_test(test_application_context_services
        test_application_context_services.cpp
        dss_app
        dss_comm_qt
        dss_network_qt
    )
endif()

if(TARGET dss_processing_opencv)
    dss_add_test(test_opencv_processing test_opencv_processing.cpp dss_processing_opencv)
endif()
