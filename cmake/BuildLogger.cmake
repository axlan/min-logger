function(build_min_logger src_dir dependant_target)
    if(${ARGC} GREATER 2)
        set(type_defs_file ${ARGV2})
        set(EXTRA_ARGS "--type_defs=${ARGV2}")
    endif()
    set(GEN_META ${CMAKE_CURRENT_BINARY_DIR}/${dependant_target}_min_logger.json)
    get_target_property(TARGET_SOURCES ${dependant_target} SOURCES)
    add_custom_command(
        OUTPUT ${GEN_META}
        COMMAND uv --project ${CMAKE_SOURCE_DIR}/python run min-logger-builder ${src_dir} --root_paths ${CMAKE_SOURCE_DIR} --json_output ${GEN_META} ${EXTRA_ARGS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${TARGET_SOURCES} ${type_defs_file}
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
