add_library(messages STATIC
        d_englsh.h
        dstrings.c
        dstrings.h
)

target_include_directories(messages PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
