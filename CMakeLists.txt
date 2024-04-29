cmake_minimum_required(VERSION 3.24)

project(gc-arena VERSION 0.2 LANGUAGES C)

add_library(gc-arena include/gc-arena/arena.h src/arena.c src/gc.c)
target_include_directories(gc-arena PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_features(gc-arena PUBLIC c_std_11)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_executable(example apps/example.c)
    target_link_libraries(example PRIVATE gc-arena)
endif()
