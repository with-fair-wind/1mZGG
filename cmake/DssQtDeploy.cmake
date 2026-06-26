include_guard(GLOBAL)

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
