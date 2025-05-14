add_library(automap STATIC
        am_map.c
        am_map.h
        am_responder.c
        am_responder.h
)

target_include_directories(automap PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(automap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(automap PRIVATE common dehacked memory messages net playsim render sha1 special statusbar time video wad)
target_link_libraries(automap PUBLIC cheat input math)
