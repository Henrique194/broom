add_library(net STATIC
        d_net.c
        net_client.c
        net_client.h
        net_common.c
        net_common.h
        net_dedicated.c
        net_dedicated.h
        net_defs.h
        net_gui.c
        net_gui.h
        net_io.c
        net_io.h
        net_loop.c
        net_loop.h
        net_packet.c
        net_packet.h
        net_petname.c
        net_petname.h
        net_query.c
        net_query.h
        net_sdl.c
        net_sdl.h
        net_server.c
        net_server.h
        net_structrw.c
        net_structrw.h
)

target_include_directories(net PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(net PRIVATE cli common config dehacked input math memory menu playsim savegame sha1 time video wad textscreen)
if(ENABLE_SDL2_NET)
    target_link_libraries(net PRIVATE SDL2_net::SDL2_net)
endif()
