#
# libsuiw
#
add_library (suiw SHARED
    softuiwarp.cpp
)
target_include_directories (suiw PUBLIC ${IWARP_INCLUDE_DIR})
