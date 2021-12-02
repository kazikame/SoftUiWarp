add_executable (read_lat 
    perftest.cpp
    read_lat.cpp
)
target_link_libraries (read_lat LINK_PRIVATE suiw)

add_executable (write_lat 
    perftest.cpp
    write_lat.cpp
)
target_link_libraries (write_lat LINK_PRIVATE suiw)

add_executable (read_bw 
    perftest.cpp
    read_bw.cpp
)
target_link_libraries (read_bw LINK_PRIVATE suiw)

add_executable (write_bw
    perftest.cpp
    write_bw.cpp
)
target_link_libraries (write_bw LINK_PRIVATE suiw)
