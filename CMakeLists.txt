cmake_minimum_required(VERSION 3.19)

project("mdb" LANGUAGES CXX ASM)
include(${CMAKE_SOURCE_DIR}/CompilerFlags.cmake)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

find_package(PkgConfig REQUIRED)
pkg_check_modules(readline REQUIRED IMPORTED_TARGET readline)
find_package(fmt CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)

include(CTest)

add_subdirectory("src")
add_subdirectory("tools")

if(BUILD_TESTING)
  find_package(Catch2 CONFIG REQUIRED)
  add_subdirectory("test")
endif()