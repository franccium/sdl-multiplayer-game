cmake_minimum_required(VERSION 3.16)
project(server)

# Set the output directory for built objects.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# This assumes the SDL source is available in vendored/SDL
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# Create your game executable target
add_executable(server src/server.c)
add_executable(client src/client.c)

# Link to the actual SDL3 library.
target_link_libraries(server PRIVATE SDL3::SDL3)
target_link_libraries(client PRIVATE SDL3::SDL3)