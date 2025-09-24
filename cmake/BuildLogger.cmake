function(build_min_logger src_dir dependant_target)
    set(GEN_META ${CMAKE_CURRENT_BINARY_DIR}/${dependant_target}_min_logger.json)
    add_custom_command(
        OUTPUT ${GEN_META}
        COMMAND uv --project ${CMAKE_SOURCE_DIR}/python run min-logger-builder ${src_dir} --root_paths ${CMAKE_SOURCE_DIR} --json_output ${GEN_META}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    # This changes the GCC __FILE__ macro to be relative to the source directory instead of using absolute paths.
    target_compile_options(${dependant_target} PRIVATE -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=)
    # This is needed so the meta data is actually generated.
    add_custom_target(
        ${dependant_target}_min_logger_meta
        DEPENDS ${GEN_META}
    )
    add_dependencies(${dependant_target} ${dependant_target}_min_logger_meta)
endfunction()
