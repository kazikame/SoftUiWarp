cmake_minimum_required (VERSION 3.10)

project (SoftUiWarp)


add_library(SoftUiWarp mpa/mpa.c common/iwarp.c)
target_include_directories (SoftUiWarp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(fake_ping_client fake_ping_client.c)
target_link_libraries (fake_ping_client LINK_PRIVATE SoftUiWarp)