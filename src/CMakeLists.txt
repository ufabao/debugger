add_library(libmdb 
              process.cpp 
              pipe.cpp 
              registers.cpp 
              breakpoint_site.cpp 
              disassembler.cpp 
              watchpoint.cpp 
              syscalls.cpp)  

              
add_library(mdb::libmdb ALIAS libmdb) 

target_link_libraries(libmdb PRIVATE Zydis::Zydis)

set_target_properties( 
    libmdb
    PROPERTIES OUTPUT_NAME mdb
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
    EXPORT mdb-targets
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
    EXPORT mdb-targets
    FILE mdb-config.cmake
    NAMESPACE mdb::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mdb
)

