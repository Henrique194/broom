add_library(common STATIC
        d_mode.c
        d_mode.h
        d_player.h
        d_ticcmd.h
        doomdata.h
        doomdef.h
        doomstat.c
        doomstat.h
        doomtype.h
        g_game.c
        g_game.h
        i_swap.h
        i_system.c
        i_system.h
        m_misc.c
        m_misc.h
)

target_include_directories(common PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(common PRIVATE automap cheat cli dehacked finale hud input intermission map memory menu messages net rand render savegame sha1 sound special stats statusbar video wad SDL2::SDL2)
target_link_libraries(common PUBLIC math playsim time)
