add_library(special STATIC
        p_ceiling.c
        p_ceiling.h
        p_doors.c
        p_doors.h
        p_floor.c
        p_floor.h
        p_lights.c
        p_lights.h
        p_plats.c
        p_plats.h
        p_spec.c
        p_spec.h
        p_switch.c
        p_switch.h
        p_telept.c
        p_telept.h
)

target_include_directories(special PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(special PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(special PRIVATE cli dehacked input math memory messages net rand sha1 sound time video wad)
target_link_libraries(special PUBLIC common playsim render)
