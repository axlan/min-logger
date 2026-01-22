
add_executable(hello_cpp hello.cpp)
target_link_libraries(hello_cpp PUBLIC min_logger)
target_link_options(hello_cpp
  PUBLIC
    LINKER:-Map=examples/hello_cpp/hello_cpp.map
)
build_min_logger(${CMAKE_CURRENT_SOURCE_DIR} hello_cpp)
