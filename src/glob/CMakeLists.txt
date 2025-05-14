set(SOURCE_FILES i_glob.c i_glob.h)

if(MSVC)
    list(APPEND SOURCE_FILES win_opendir.c win_opendir.h)
endif()

add_library(glob STATIC ${SOURCE_FILES})
target_include_directories(glob PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(glob PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(glob PRIVATE common)
