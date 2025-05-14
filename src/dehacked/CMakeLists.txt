add_library(dehacked STATIC
        deh_ammo.c
        deh_bexstr.c
        deh_cheat.c
        deh_defs.h
        deh_doom.c
        deh_frame.c
        deh_io.c
        deh_io.h
        deh_main.c
        deh_main.h
        deh_mapping.c
        deh_mapping.h
        deh_misc.c
        deh_misc.h
        deh_ptr.c
        deh_sound.c
        deh_str.c
        deh_str.h
        deh_text.c
        deh_thing.c
        deh_weapon.c
)

target_include_directories(dehacked PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(dehacked PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dehacked PRIVATE automap cli glob input math memory messages playsim render sound special statusbar time video wad)
target_link_libraries(dehacked PUBLIC common sha1)
