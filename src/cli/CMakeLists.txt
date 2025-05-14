add_library(cli STATIC
        m_argv.c
        m_argv.h
)

target_include_directories(cli PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cli PRIVATE common wad SDL2::SDL2)
