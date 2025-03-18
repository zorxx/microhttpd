set(target microhttpd_example)

enable_c_compiler_flag_if_supported("-no-pedantic")
add_executable(${target} main.c)
target_link_libraries(${target} microhttpd)

install(TARGETS ${target} DESTINATION bin/microhttpd)
install(FILES index.html helpers.js DESTINATION bin/microhttpd)
