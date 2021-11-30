add_executable (perftest 
    get_clock.cpp
    perftest.cpp
)
set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
target_link_libraries (perftest PRIVATE Threads::Threads)
target_link_libraries (perftest LINK_PRIVATE suiw)
