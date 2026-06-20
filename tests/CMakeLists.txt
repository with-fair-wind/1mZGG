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
dss_add_test(test_local_image_storage_backend test_local_image_storage_backend.cpp dss_core)
dss_add_test(test_bmp_image_format test_bmp_image_format.cpp dss_core)
dss_add_test(test_result_packet_utils test_result_packet_utils.cpp dss_core)
dss_add_test(test_track_data_storage_format test_track_data_storage_format.cpp dss_core)
dss_add_test(test_track_data_storage_backend test_track_data_storage_backend.cpp dss_core)
dss_add_test(test_data_exchange_protocol test_data_exchange_protocol.cpp dss_core)
dss_add_test(test_network_protocols test_network_protocols.cpp dss_core)
dss_add_test(test_config test_config.cpp dss_core)
dss_add_test(test_logger test_logger.cpp dss_core)
dss_add_test(test_service_host test_service_host.cpp dss_core)
dss_add_test(test_service_registry test_service_registry.cpp dss_core)
dss_add_test(test_track_result_data_exchange_bridge test_track_result_data_exchange_bridge.cpp dss_app)
dss_add_test(test_observation_session test_observation_session.cpp dss_app)
dss_add_test(test_runtime_diagnostics test_runtime_diagnostics.cpp dss_app)
dss_add_test(test_math_utils test_math_utils.cpp dss_tracking)
dss_add_test(test_tracking_candidate_utils test_tracking_candidate_utils.cpp dss_tracking)
dss_add_test(test_tracking_lifecycle_utils test_tracking_lifecycle_utils.cpp dss_tracking)
dss_add_test(test_tracking_prediction_utils test_tracking_prediction_utils.cpp dss_tracking)
dss_add_test(test_track_manager test_track_manager.cpp dss_tracking)
dss_add_test(test_manual_tracker test_manual_tracker.cpp dss_tracking)
dss_add_test(test_leo_tracker test_leo_tracker.cpp dss_tracking)
dss_add_test(test_geo_tracker test_geo_tracker.cpp dss_tracking)
dss_add_test(test_sc_tracker test_sc_tracker.cpp dss_tracking)
dss_add_test(test_tracking_legacy_scenarios test_tracking_legacy_scenarios.cpp dss_tracking)
target_sources(test_tracking_legacy_scenarios PRIVATE tracking_fixture_loader.cpp)
target_include_directories(test_tracking_legacy_scenarios PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(test_tracking_legacy_scenarios PRIVATE
    DSS_TRACKING_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tracking"
)
dss_add_test(test_bounded_channel test_bounded_channel.cpp dss_processing)
dss_add_test(test_display_stretch test_display_stretch.cpp dss_processing)
dss_add_test(test_image_processor test_image_processor.cpp dss_processing)
dss_add_test(test_frame_view test_frame_view.cpp dss_processing)
dss_add_test(test_processing_pipeline test_processing_pipeline.cpp dss_processing)
dss_add_test(test_diff_processing test_diff_processing.cpp dss_processing)

if(TARGET dss_network_qt)
    dss_add_test(test_error_diagnostics test_error_diagnostics.cpp dss_network_qt)
    dss_add_test(test_data_exchange test_data_exchange.cpp dss_network_qt)
    dss_add_test(test_image_sender test_image_sender.cpp dss_network_qt)
    dss_add_test(test_heartbeat test_heartbeat.cpp dss_network_qt)
endif()

if(TARGET dss_comm_qt AND TARGET dss_network_qt)
    dss_add_qt_test(test_application_context_services
        test_application_context_services.cpp
        dss_app
        dss_comm_qt
        dss_network_qt
        dss_acquisition_qt
        dss_processing
        Qt6::Gui
    )
endif()

if(TARGET dss_acquisition_qt)
    dss_add_test(test_frame_source_coordinator
        test_frame_source_coordinator.cpp
        dss_acquisition_qt
    )
    dss_add_test(test_serial_camera_controller
        test_serial_camera_controller.cpp
        dss_acquisition_qt
    )
    dss_add_test(test_sapera_frame_source_contract
        test_sapera_frame_source_contract.cpp
        dss_acquisition_qt
    )
    dss_add_qt_test(test_image_sequence_frame_source
        test_image_sequence_frame_source.cpp
        dss_acquisition_qt
    )
endif()

if(TARGET dss_ui_qt)
    dss_add_qt_test(test_image_display
        test_image_display.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_log_palette
        test_log_palette.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_log_view_model
        test_log_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_display_view_model
        test_display_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_replay_view_model
        test_replay_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_processing_view_model
        test_processing_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_tracking_view_model
        test_tracking_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_settings_view_model
        test_settings_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_storage_view_model
        test_storage_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_serial_port_view_model
        test_serial_port_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_network_view_model
        test_network_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_data_exchange_view_model
        test_data_exchange_view_model.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_main_window_layout
        test_main_window_layout.cpp
        dss_ui_qt
    )
    dss_add_qt_test(test_main_view_model
        test_main_view_model.cpp
        dss_ui_qt
    )
endif()

if(TARGET dss_processing_opencv)
    dss_add_test(test_opencv_processing test_opencv_processing.cpp dss_processing_opencv)
endif()
