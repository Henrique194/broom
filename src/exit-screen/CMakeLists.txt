add_library(exitscreen STATIC
        i_endoom.c
        i_endoom.h
)

target_include_directories(exitscreen PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(exitscreen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(exitscreen PRIVATE common dehacked memory wad textscreen)
