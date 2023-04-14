#
# see: https://stackoverflow.com/questions/17511496/how-to-create-a-shared-library-with-cmake
#
SET (THE_PROJECT libmemvar)

cmake_minimum_required(VERSION 3.26)

PROJECT(${THE_PROJECT} VERSION 1.0.0 DESCRIPTION "libmemvar: a lib for memvar's")

SET (LIBRARY_NAME memvar)

# C++ version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET (CMAKE_EXPORT_COMPILE_COMMANDS on)

SET (CMAKE_VERBOSE_MAKEFILE on )
SET (BUILD_SHARED_LIBS ON)

# Set the variable source_files to the list of names of your C++ source code
# Note the lack of commas or other delimiters
set(source_files
   ../memvar.cpp
)

SET(CMAKE_BUILD_TYPE Release) # Debug, Release, RelWithDebInfo, MinSizeRel
#ADD_LIBRARY(${LIBRARY_NAME} STATIC src/memvar.cpp)
ADD_LIBRARY(${LIBRARY_NAME} ../memvar.cpp) # shared lib
SET_TARGET_PROPERTIES(${LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
SET_TARGET_PROPERTIES(${LIBRARY_NAME} PROPERTIES SOVERSION 1)
SET_TARGET_PROPERTIES(${LIBRARY_NAME} PROPERTIES PUBLIC_HEADER src/memvar.h)

