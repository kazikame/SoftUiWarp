#
# suiw
#
add_library (suiw SHARED
    ddp/ddp.cpp
    mpa/mpa.cpp
    mpa/printer.cpp
    rdmap/rdmap.cpp
    ../common/cq.cpp
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(suiw PRIVATE Threads::Threads)

target_include_directories (suiw PUBLIC ../common/)
target_include_directories (suiw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})