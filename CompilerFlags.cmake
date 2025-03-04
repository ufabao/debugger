# CompilerFlags.cmake - Include this from your main CMakeLists.txt

# Common flags for both Debug and Release builds
add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wshadow
  -Wconversion
  -Wsign-conversion
  -Wnull-dereference
  -Wdouble-promotion
  -Wformat=2
)

# Debug-specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(
    -g
    -O0
    # Uncomment to enable sanitizers during development
    # -fsanitize=address,undefined
  )
  # Uncomment if using sanitizers
  # add_link_options(-fsanitize=address,undefined)
endif()

# Release-specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(
    -O3
    # Still include debug info in release for better debugging support
    -g1
  )
endif()