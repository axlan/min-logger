
add_executable(hello_cpp hello.cpp)
target_link_libraries(hello_cpp PUBLIC min_logger)
build_min_logger(${CMAKE_CURRENT_SOURCE_DIR} hello_cpp)
