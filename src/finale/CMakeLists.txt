add_library(finale STATIC
        f_finale.c
        f_finale.h
)

target_include_directories(finale PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(finale PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(finale PRIVATE common dehacked hud memory messages net render sound video wad SDL2::SDL2)
target_link_libraries(finale PUBLIC input)
