add_executable(hello_c hello.c)
target_link_libraries(hello_c PUBLIC min_logger)
# target_compile_options(hello_c PRIVATE -DMIN_LOGGER_ENABLED=0)
build_min_logger(${CMAKE_CURRENT_SOURCE_DIR} hello_c)
