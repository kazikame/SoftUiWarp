add_executable (read_lat 
    perftest.cpp
    read_lat.cpp
)
target_link_libraries (read_lat LINK_PRIVATE suiw)
set_property(TARGET read_lat PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable (write_lat 
    perftest.cpp
    write_lat.cpp
)
target_link_libraries (write_lat LINK_PRIVATE suiw)
set_property(TARGET write_lat PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable (read_bw 
    perftest.cpp
    read_bw.cpp
)
target_link_libraries (read_bw LINK_PRIVATE suiw)
set_property(TARGET read_bw PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable (write_bw
    perftest.cpp
    write_bw.cpp
)
target_link_libraries (write_bw LINK_PRIVATE suiw)
set_property(TARGET write_bw PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
