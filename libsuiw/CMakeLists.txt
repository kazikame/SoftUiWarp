#
# libsuiw
#
add_library (suiw SHARED
    softuiwarp.cpp
)
target_include_directories (suiw PUBLIC ${IWARP_INCLUDE_DIR})
target_link_libraries (suiw rdmacm)

#
# libsuiw daemon process
#
add_executable (libsuiwd 
    libsuiwd.cpp
)
target_link_libraries (libsuiwd rdmacm)
