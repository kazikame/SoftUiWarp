#
# librdmacm
#
add_library (rdmacm_interpose SHARED
    rdma_cma.cpp    
)
target_include_directories (rdmacm_interpose PUBLIC 
    ${IWARP_INCLUDE_DIR}
    ../libsuiw
)
target_link_libraries (rdmacm_interpose 
    dl
    suiw 
)
