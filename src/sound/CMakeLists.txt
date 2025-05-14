add_library(sound STATIC
        gusconf.c
        gusconf.h
        i_flmusic.c
        i_musicpack.c
        i_oplmusic.c
        i_pcsound.c
        i_sdlmusic.c
        i_sdlsound.c
        i_sound.c
        i_sound.h
        i_winmusic.c
        midifallback.c
        midifallback.h
        midifile.c
        midifile.h
        mus2mid.c
        mus2mid.h
        sounds.c
        sounds.h
        s_sound.c
        s_sound.h
)

target_include_directories(sound PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(sound PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sound PRIVATE cli common config dehacked glob input math memory net playsim rand render sha1 special time video wad opl pcsound samplerate SDL2::SDL2)
if(ENABLE_SDL2_MIXER)
    target_link_libraries(sound PRIVATE SDL2_mixer::SDL2_mixer)
endif()
