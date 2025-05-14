add_library(video STATIC
        i_video.c
        i_video.h
        v_diskicon.c
        v_diskicon.h
        v_icon.c
        v_icon.h
        v_patch.h
        v_video.c
        v_video.h
)

target_include_directories(video PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(video PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(video PRIVATE cli common config dehacked input math memory net sha1 time wad SDL2::SDL2)
