add_library(screenmelt STATIC
        f_wipe.c
        f_wipe.h
)

target_include_directories(screenmelt PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(screenmelt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(screenmelt PRIVATE common memory rand video)
