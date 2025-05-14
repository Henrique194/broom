add_library(render STATIC
        r_border.c
        r_border.h
        r_bsp.c
        r_bsp.h
        r_column.c
        r_column.h
        r_data.c
        r_data.h
        r_defs.h
        r_fuzz_column.c
        r_fuzz_column.h
        r_local.h
        r_main.c
        r_main.h
        r_plane.c
        r_plane.h
        r_player_column.c
        r_player_column.h
        r_screen.c
        r_screen.h
        r_segs.c
        r_segs.h
        r_sky.c
        r_sky.h
        r_span.c
        r_span.h
        r_state.h
        r_things.c
        r_things.h
)

target_include_directories(render PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(render PRIVATE common dehacked input map math memory menu net playsim savegame sha1 special time video wad SDL2::SDL2)
