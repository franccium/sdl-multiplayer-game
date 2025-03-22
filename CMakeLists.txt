cmake_minimum_required(VERSION 3.16)
project(sdl-multiplayer-game)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(SDL EXCLUDE_FROM_ALL)

include_directories(src)

set(COMMON_SOURCES
    src/common/common.h
)

add_executable(server
    src/server/server.c
    src/server/server.h
    ${COMMON_SOURCES}
)

add_executable(client
    src/client/client.c
    src/client/client.h
    src/game_window.c
    src/game_window.h
    ${COMMON_SOURCES}
)

target_link_libraries(client PRIVATE SDL3::SDL3)