
add_executable(hello_c hello.c)
target_link_libraries(hello_c PUBLIC min_logger example_utils)
build_min_logger(${CMAKE_CURRENT_SOURCE_DIR} hello_c)
