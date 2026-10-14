cmake_minimum_required(VERSION 3.14)
project(memvar_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(MEMVAR_ENABLE_COVERAGE "Build with --coverage instrumentation (gcc/clang)" OFF)

# How memvar<T>::cref() protects against use after eviction (see memvar.h):
#   0 = abort, 1 = throw mv::expired_history_reference, 2 = no check (default).
# The test suite adapts to the selected mode, so configure it once per mode.
set(MEMVAR_SAFEREF_CHECK 2 CACHE STRING "cref() safety mode: 0=abort, 1=throw, 2=no check")
set_property(CACHE MEMVAR_SAFEREF_CHECK PROPERTY STRINGS 0 1 2)
if(NOT MEMVAR_SAFEREF_CHECK MATCHES "^[012]$")
  message(FATAL_ERROR "MEMVAR_SAFEREF_CHECK must be 0, 1 or 2 (got '${MEMVAR_SAFEREF_CHECK}')")
endif()

# --------------------------------------------------------------------------
# GoogleTest: prefer an already-installed system package (e.g. libgtest-dev
# on Ubuntu/Debian), and fall back to downloading the sources via
# FetchContent if no system package is found.
# --------------------------------------------------------------------------
find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
  )
  # Keep gtest's own settings from clashing with ours.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  set(MEMVAR_GTEST_LIBS gtest gtest_main)
else()
  set(MEMVAR_GTEST_LIBS GTest::gtest GTest::gtest_main)
endif()

add_executable(memvar_tests memvar_test.cpp)

target_include_directories(memvar_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(memvar_tests PRIVATE
  ${MEMVAR_GTEST_LIBS}
  pthread
)

target_compile_definitions(memvar_tests PRIVATE
  MEMVAR_SAFEREF_CHECK=${MEMVAR_SAFEREF_CHECK}
)

target_compile_options(memvar_tests PRIVATE
  -Wall -Wextra
)

if(MEMVAR_ENABLE_COVERAGE)
  # MEMVAR_TEST_GCOV_DUMP lets the death-test children of mode 0 write their
  # coverage counters before they abort.
  target_compile_definitions(memvar_tests PRIVATE MEMVAR_TEST_GCOV_DUMP)
  target_compile_options(memvar_tests PRIVATE --coverage -O0 -g)
  target_link_options(memvar_tests PRIVATE --coverage)
endif()

enable_testing()
include(GoogleTest)
gtest_discover_tests(memvar_tests)

message(STATUS "memvar tests: MEMVAR_SAFEREF_CHECK=${MEMVAR_SAFEREF_CHECK}")

