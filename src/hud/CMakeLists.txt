add_library(hud STATIC
        hu_lib.c
        hu_lib.h
        hu_stuff.c
        hu_stuff.h
)

target_include_directories(hud PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(hud PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hud PRIVATE common dehacked input math memory menu messages net playsim render savegame sha1 sound time wad SDL2::SDL2)
target_link_libraries(hud PUBLIC video)
