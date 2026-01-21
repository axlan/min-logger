
add_executable(custom_type custom_type.cpp)
target_link_libraries(custom_type PUBLIC min_logger example_utils)
target_link_options(custom_type
  PUBLIC
    LINKER:-Map=examples/custom_type/custom_type.map
)
build_min_logger(${CMAKE_CURRENT_SOURCE_DIR} custom_type ${CMAKE_CURRENT_SOURCE_DIR}/custom_type_type_defs.json)
