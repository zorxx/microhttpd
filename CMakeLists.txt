cmake_minimum_required(VERSION 3.0)
set(project microhttpd)
project(${project})

option(BUILD_TESTS "Build test programs" OFF)
option(DEBUG_PRINT "Enable library debug print" OFF)

add_library(${project} client.c helpers.c microhttpd.c post.c)
target_include_directories(${project} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(DEBUG_PRINT)
   target_compile_definitions(${project} PRIVATE DEBUG)
endif()

if(BUILD_TESTS)
   add_subdirectory(test)
endif()

install(TARGETS ${project} DESTINATION lib) 
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/microhttpd DESTINATION include)
