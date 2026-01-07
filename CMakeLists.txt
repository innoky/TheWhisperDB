cmake_minimum_required(VERSION 3.20)

project(whisperdb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)

add_executable(server
    src/main.cpp
    src/core/GraphDB.cpp
    src/core/GNode.cpp
    src/http/MultipartParser.cpp
    src/server/wserver.cpp
    src/server/FileStorage.cpp
    src/server/UploadHandler.cpp
)

target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_options(server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
