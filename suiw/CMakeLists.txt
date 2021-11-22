#
# suiw
#
add_library (suiw SHARED
   ddp.cpp
)
target_include_directories (suiw PUBLIC ${IWARP_INCLUDE_DIR})

