#
SET (THE_PROJECT memvar-unit-tests)

cmake_minimum_required(VERSION 3.26)

PROJECT(${THE_PROJECT} VERSION 1.0.0 DESCRIPTION "unit tests for memvar")

# C++ version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET (CMAKE_EXPORT_COMPILE_COMMANDS on)
SET (CMAKE_VERBOSE_MAKEFILE on )

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

find_package(GMock REQUIRED)
include_directories(${GMOCK_INCLUDE_DIRS})

# Set the variable source_files to the list of names of your C++ source code
# Note the lack of commas or other delimiters
SET(source_files
   unitTests.cpp
   bigint.cpp
)

SET(CMAKE_BUILD_TYPE Release) # Debug, Release, RelWithDebInfo, MinSizeRel

# Build a program called '${THE_PROJECT}' from the source files we specified above
ADD_EXECUTABLE(${THE_PROJECT} ${source_files})

TARGET_LINK_LIBRARIES(${THE_PROJECT} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${GMOCK_LIBRARIES} ${GMOCK_MAIN_LIBRARIES} )

