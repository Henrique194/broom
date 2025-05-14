add_library(math STATIC
        m_fixed.c
        m_fixed.h
        tables.c
        tables.h
)

target_include_directories(math PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(math PRIVATE common)
