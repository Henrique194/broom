add_library(rand STATIC
        m_random.c
        m_random.h
)

target_include_directories(rand PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(rand PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
