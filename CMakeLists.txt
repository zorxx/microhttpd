cmake_minimum_required(VERSION 3.0)
set(project microhttpd)
project(${project})

add_library(${project} client.c helpers.c microhttpd.c post.c)
target_include_directories(${project} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(test)

install(TARGETS ${project} DESTINATION lib) 
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/microhttpd DESTINATION include)
