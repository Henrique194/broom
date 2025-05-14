add_library(intermission STATIC
        wi_stuff.c
        wi_stuff.h
)

target_include_directories(intermission PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(intermission PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(intermission PRIVATE dehacked input math memory playsim net rand render sound time video wad SDL2::SDL2)
target_link_libraries(intermission PUBLIC common)
