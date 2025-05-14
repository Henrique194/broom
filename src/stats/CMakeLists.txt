add_library(stats STATIC
        statdump.c
        statdump.h
)

target_include_directories(stats PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(stats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(stats PRIVATE cli common math playsim time)
