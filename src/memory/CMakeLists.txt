add_library(memory STATIC
        memio.c
        memio.h
        z_zone.c
        z_zone.h
)

target_include_directories(memory PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(memory PRIVATE cli common)
