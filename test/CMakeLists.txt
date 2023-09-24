set(target microhttpd_test)
add_executable(${target} main.c)
target_link_libraries(${target} microhttpd)

install(TARGETS ${target} DESTINATION bin/microhttpd)
install(FILES index.html helpers.js DESTINATION bin/microhttpd)
