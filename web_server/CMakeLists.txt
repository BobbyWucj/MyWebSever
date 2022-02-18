file(GLOB SRC "*.*")

add_library(webserver STATIC ${SRC})

target_link_libraries(webserver pthread)

install(TARGETS webserver DESTINATION lib)
install(FILES ${SRC} DESTINATION include)
