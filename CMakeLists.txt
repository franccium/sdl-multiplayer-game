cmake_minimum_required(VERSION 3.16)
project(sdl-multiplayer-game)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(SDL EXCLUDE_FROM_ALL)
find_package(OpenGL REQUIRED)
add_subdirectory(cglm EXCLUDE_FROM_ALL)

include_directories(src)
include_directories(cglm/include)

set(COMMON_SOURCES
    src/common/common.h
    src/common/client_common.h
    src/common/collisions.h
    src/common/collisions.c
    src/common/hashset.h
    src/common/hashset.c
)

add_executable(server
    ${COMMON_SOURCES}
    src/server/server.h
    src/server/server.c
)

add_executable(client
    ${COMMON_SOURCES}
    src/client/client.c
    src/game_window.h
    src/game_window.c
)

target_link_libraries(client PRIVATE SDL3::SDL3 m OpenGL::GL)
target_link_libraries(server PRIVATE SDL3::SDL3 cglm m)
