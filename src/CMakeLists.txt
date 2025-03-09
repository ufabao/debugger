add_library(libmdb process.cpp pipe.cpp registers.cpp)
add_library(sdb::libmdb ALIAS libmdb)

set_target_properties(
  libmdb
  PROPERTIES OUTPUT_NAME sdb
)

target_compile_features(libmdb PUBLIC cxx_std_17)

target_include_directories(libmdb
  PUBLIC 
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/include
)

include(GNUInstallDirs)
install(TARGETS libmdb 
  EXPORT sdb-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}  
)

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT sdb-targets 
  FILE sdb-config.cmake 
  NAMESPACE sdb::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sdb
)