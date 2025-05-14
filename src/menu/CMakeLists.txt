add_library(menu STATIC
        m_menu.c
        m_menu.h
        m_menu_responder.c
        m_menu_responder.h
)

target_include_directories(menu PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(menu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(menu PRIVATE cli common dehacked hud input map math memory messages playsim net render savegame sha1 sound time video wad SDL2::SDL2)
