#
SET (THE_PROJECT memvar-unit-tests)

cmake_minimum_required(VERSION 3.26)

#set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER g++)  # g++ shows better performance than clang++ on ubuntu 24.04.1 LTS

PROJECT(${THE_PROJECT} VERSION 1.0.0 DESCRIPTION "unit tests for memvar")

# C++ version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET (CMAKE_EXPORT_COMPILE_COMMANDS on)
SET (CMAKE_VERBOSE_MAKEFILE on )

SET(CMAKE_BUILD_TYPE Release) # Debug, Release, RelWithDebInfo, MinSizeRel

# Set the build type (Debug, Release, RelWithDebInfo, MinSizeRel)
if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
endif()

# Set more compiler flags for debug mode; -g is automatically set
if(CMAKE_BUILD_TYPE MATCHES Debug)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

# Set more compiler flags for release mode; -O3 is automatically set
if(CMAKE_BUILD_TYPE MATCHES Release)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

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

# Build a program called '${THE_PROJECT}' from the source files we specified above
ADD_EXECUTABLE(${THE_PROJECT} ${source_files})

TARGET_LINK_LIBRARIES(${THE_PROJECT} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${GMOCK_LIBRARIES} ${GMOCK_MAIN_LIBRARIES} )
