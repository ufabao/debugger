
add_executable(mdb mdb.cpp)
target_link_libraries(mdb PRIVATE mdb::libmdb PkgConfig::readline)

include(GNUInstallDirs)
install(
  TARGETS mdb 
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)