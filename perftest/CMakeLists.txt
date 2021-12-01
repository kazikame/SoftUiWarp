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

