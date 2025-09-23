add_executable(cpp_crc_test cpp_crc_test.cpp)
target_link_libraries(cpp_crc_test PRIVATE min_logger)
add_test(NAME cpp_crc_test COMMAND cpp_crc_test)

add_executable(c_crc_test c_crc_test.c)
target_link_libraries(c_crc_test PRIVATE min_logger)
add_test(NAME c_crc_test COMMAND c_crc_test)
