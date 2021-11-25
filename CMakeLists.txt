cmake_minimum_required (VERSION 3.10)

project (SoftUiWarp VERSION 1 DESCRIPTION "SoftiWARP in Userspace")

# Output library binaries to a ./lib directory.
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# iWARP include and library path.
set (IWARP_DIR "~/softiwarp-user-for-linux-rdma/build")
set (IWARP_INCLUDE_DIR "${IWARP_DIR}/include")
set (IWARP_LIB_DIR "${IWARP_DIR}/lib")

# Add libraries to compile.
add_subdirectory (libsuiw)
add_subdirectory (libibverbs)
add_subdirectory (librdmacm)

add_library (SoftUiWarp suiw/ddp.cpp mpa/mpa.cpp common/iwarp.cpp suiw/rdmap.cpp)
target_include_directories (SoftUiWarp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable (fake_ping_client fake_ping_client.cpp)
target_link_libraries (fake_ping_client LINK_PRIVATE SoftUiWarp)
