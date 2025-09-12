
add_executable(log_threads log_threads.cpp)
target_link_libraries(log_threads PUBLIC min_logger)
build_min_logger(${CMAKE_CURRENT_SOURCE_DIR} log_threads)
