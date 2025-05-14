add_library(sha1 STATIC
        sha1.c
        sha1.h
)

target_include_directories(sha1 PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(sha1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sha1 PRIVATE common)
