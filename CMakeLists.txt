cmake_minimum_required(VERSION 3.25)
project(our_taskdepot_foundation LANGUAGES C)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This foundation targets Windows x64 only.")
endif()
if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "The application foundation must be built with MSVC.")
endif()

include(CTest)
include(cmake/Dependencies.cmake)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
