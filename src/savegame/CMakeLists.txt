add_library(savegame STATIC
        p_saveg.c
        p_saveg.h
)

target_include_directories(savegame PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(savegame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(savegame PRIVATE common dehacked input math memory net messages playsim render sha1 special time video)
