
add_executable(sdb mdb.cpp)
target_link_libraries(sdb PRIVATE sdb::libmdb PkgConfig::readline fmt::fmt)

include(GNUInstallDirs)
install(
  TARGETS sdb 
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)