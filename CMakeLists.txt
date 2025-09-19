# Copyright (C) Point One Navigation - All Rights Reserved
cmake_minimum_required(VERSION 3.12)

# Set toolchain parameters before calling project().
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define user options.
option(MIN_LOGGER_BUILD_EXAMPLES "Build example applications." ON)

if (NOT DEFINED BUILD_SHARED_LIBS)
    option(BUILD_SHARED_LIBS
           "Build shared libraries instead of static libraries."
           ON)
endif()

# Define the project and setup the compiler toolchain. This will establish
# default compiler/linker flags. If the user specifies a cross-compilation
# toolchain (-DCMAKE_TOOLCHAIN_FILE=...), it will be applied now.
project(min_logger VERSION 1.0.0)

# Set additional compilation flags.
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Werror)
endif()

################################################################################
# Library Definitions
################################################################################

# Define the min_logger library and supporting code.
add_library(min_logger
            src/min_logger/min_logger.cpp
            )
target_include_directories(min_logger PUBLIC ${PROJECT_SOURCE_DIR}/src)
if (MSVC)
    target_compile_definitions(min_logger PRIVATE BUILDING_DLL)
endif()

set_target_properties(min_logger PROPERTIES
                      VERSION ${PROJECT_VERSION}
                      SOVERSION ${PROJECT_VERSION_MAJOR})

# Install targets.
install(TARGETS min_logger
        LIBRARY DESTINATION lib)

install(DIRECTORY src/min_logger DESTINATION include
        FILES_MATCHING PATTERN "*.h")

include(cmake/BuildLogger.cmake)

################################################################################
# Example Applications (Optional)
################################################################################

if (MIN_LOGGER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
