cmake_minimum_required(VERSION 3.14)
project(whisperdb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server
    src/main.cpp
    src/core/GraphDB.cpp
    src/core/GNode.cpp
    src/server/wserver.cpp
)

target_compile_options(server PRIVATE -fcoroutines)
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)

target_include_directories(server PRIVATE 
    /usr/include
    ${CMAKE_SOURCE_DIR}/include
    )

