# Copyright 2023-2025 Zorxx Software. All rights reserved.

# esp-idf component
if(IDF_TARGET)
   idf_component_register(SRCS "src/client.c" "src/helpers.c" "src/microhttpd.c" "src/post.c"
                          PRIV_INCLUDE_DIRS "./src"
                          INCLUDE_DIRS "./include")
   return()
endif()

cmake_minimum_required(VERSION 3.5)
project(microhttpd LANGUAGES C VERSION 1.0.0)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(MICROHTTPD_BUILD_TESTS "Build test programs" OFF)
option(MICROHTTPD_BUILD_EXAMPLES "Build example programs" OFF)
option(MICROHTTPD_DEBUG_PRINT "Enable library debug print" OFF)

include(cmake/helpers.cmake)
enable_c_compiler_flag_if_supported("-Wall")
enable_c_compiler_flag_if_supported("-Wextra")
enable_c_compiler_flag_if_supported("-Wimplicit-fallthrough")
enable_c_compiler_flag_if_supported("-pedantic")

set(SRC src/client.c src/helpers.c src/microhttpd.c src/post.c)
add_library(${PROJECT_NAME} ${SRC})
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(DEBUGMICROHTTPD_DEBUG_PRINT_PRINT)
   target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

if(MICROHTTPD_BUILD_TESTS)
   add_subdirectory(test)
endif()
if(MICROHTTPD_BUILD_EXAMPLES)
   add_subdirectory(example)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/microhttpd DESTINATION include)
