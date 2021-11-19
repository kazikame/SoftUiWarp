#
# libibverbs
#
add_library (ibverbs_interpose SHARED
    verbs.cpp    
)
target_include_directories (ibverbs_interpose PUBLIC 
    ${IWARP_INCLUDE_DIR}
    ../libsuiw
)
target_link_libraries (ibverbs_interpose
    dl
    suiw 
)
