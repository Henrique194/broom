add_library(time STATIC
        i_timer.c
        i_timer.h
)

target_include_directories(time PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(time PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(time PRIVATE SDL2::SDL2)
