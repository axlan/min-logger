
add_executable(binary_cpp binary_cpp.cpp)
target_link_libraries(binary_cpp PUBLIC min_logger)
target_link_options(hello_cpp
  PUBLIC
    LINKER:-Map=examples/binary_cpp/binary_cpp.map
)
build_min_logger(${CMAKE_CURRENT_SOURCE_DIR} binary_cpp)
