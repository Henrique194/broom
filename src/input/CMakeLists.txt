add_library(input STATIC
        d_event.c
        d_event.h
        doomkeys.h
        i_input.c
        i_input.h
        i_joystick.c
        i_joystick.h
        m_controls.c
        m_controls.h
)

target_include_directories(input PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(input PRIVATE common config textscreen)
