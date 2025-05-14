add_library(statusbar STATIC
        st_lib.c
        st_lib.h
        st_responder.c
        st_responder.h
        st_stuff.c
        st_stuff.h
)

target_include_directories(statusbar PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(statusbar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(statusbar PRIVATE automap cheat common dehacked input math memory messages net playsim rand render sha1 sound time video wad SDL2::SDL2)
