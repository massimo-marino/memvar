#
SET (THE_PROJECT memvar-unit-tests)

CMAKE_MINIMUM_REQUIRED(VERSION 3.26)

SET(CMAKE_C_COMPILER gcc)
SET(CMAKE_CXX_COMPILER g++)  # choose g++ or clang++; g++ shows better performance than clang++ on ubuntu 24.04.1 LTS

PROJECT(${THE_PROJECT} VERSION 1.4.0 DESCRIPTION "unit tests for memvar")

# Provide a default value for the preprocessor variable if not specified via CLI
if(NOT DEFINED MEMVAR_SAFEREF_CHECK)
    SET(MEMVAR_SAFEREF_CHECK 2) # Default to 2 (MEMVAR_SAFEREF_NO_CHECK)
endif()

# Register a status message so you can see it working during configuration
MESSAGE(STATUS "Building with MEMVAR_SAFEREF_CHECK=${MEMVAR_SAFEREF_CHECK}")

# C++ version
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)
# Instruct CMake to prefer the compiler flag -pthread over -lpthread
SET(THREADS_PREFER_PTHREAD_FLAG ON)

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
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -march=native")
endif()

# Locate the system's threading library
FIND_PACKAGE(Threads REQUIRED)

FIND_PACKAGE(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Set the variable SOURCE_FILES to the list of names of your C++ source code
# Note the lack of commas or other delimiters
SET(SOURCE_FILES
   unitTests.cpp
   bigint.cpp
)

# Build a program called '${THE_PROJECT}' from the source files we specified above
ADD_EXECUTABLE(${THE_PROJECT} ${SOURCE_FILES})

TARGET_COMPILE_DEFINITIONS(${THE_PROJECT} PRIVATE MEMVAR_SAFEREF_CHECK=${MEMVAR_SAFEREF_CHECK})

TARGET_LINK_LIBRARIES(${THE_PROJECT} PRIVATE Threads::Threads ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} )

