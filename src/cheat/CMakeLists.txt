add_library(cheat STATIC
        m_cheat.c
        m_cheat.h
)

target_include_directories(cheat PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(cheat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
