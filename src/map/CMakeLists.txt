add_library(map STATIC
        m_bbox.c
        m_bbox.h
        p_setup.c
        p_setup.h
)

target_include_directories(map PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(map PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(map PRIVATE dehacked cli common input memory net playsim render sha1 sound special time video SDL2::SDL2)
target_link_libraries(map PUBLIC math wad)
