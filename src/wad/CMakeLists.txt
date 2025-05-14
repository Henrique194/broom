add_library(wad STATIC
        d_iwad.c
        d_iwad.h
        w_checksum.c
        w_checksum.h
        w_file.c
        w_file.h
        w_file_posix.c
        w_file_stdc.c
        w_file_win32.c
        w_main.c
        w_main.h
        w_merge.c
        w_merge.h
        w_wad.c
        w_wad.h
)

target_include_directories(wad PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(wad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wad PRIVATE cli common config dehacked input glob memory sha1 video SDL2::SDL2)
