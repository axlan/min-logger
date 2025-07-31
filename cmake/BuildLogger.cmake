function(build_min_logger src_dir dependant_target)
    set(GEN_HEADER ${CMAKE_CURRENT_BINARY_DIR}/_min_logger_gen.h)
    add_custom_command(
        OUTPUT ${GEN_HEADER}
        COMMAND uv --project ${CMAKE_SOURCE_DIR}/python run min-logger-builder ${src_dir} --header_output ${GEN_HEADER}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target(
        ${dependant_target}_min_logger_header
        DEPENDS ${GEN_HEADER}
    )
    add_dependencies(${dependant_target} ${dependant_target}_min_logger_header)
endfunction()
