add_library(playsim STATIC
        a_enemy.c
        a_enemy.h
        a_player.h
        d_items.c
        d_items.h
        d_think.h
        info.c
        info.h
        p_inter.c
        p_inter.h
        p_local.h
        p_map.c
        p_maputl.c
        p_mobj.c
        p_mobj.h
        p_pspr.c
        p_pspr.h
        p_sight.c
        p_tick.c
        p_tick.h
        p_user.c
)

target_include_directories(playsim PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(playsim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(playsim PRIVATE automap cli dehacked input hud map memory messages net rand render sound special statusbar time video)
target_link_libraries(playsim PUBLIC common math)
