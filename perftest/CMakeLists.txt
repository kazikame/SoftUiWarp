add_executable (read_lat 
    perftest.cpp
    read_lat.cpp
)
target_link_libraries (read_lat LINK_PRIVATE suiw)
