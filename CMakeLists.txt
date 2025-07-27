cmake_minimum_required(VERSION 3.14)
project(CrowExample)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем vcpkg toolchain (если есть)

if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Добавляем исполняемый файл
add_executable(server
    src/main.cpp
    src/core/GraphDB.cpp
    src/core/GNode.cpp
)
target_compile_options(server PRIVATE -fcoroutines)
# Подключаем pthread
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)

# Указываем компилятору, где искать заголовки Crow
# Crow — это header-only библиотека, и достаточно include-пути
target_include_directories(server PRIVATE
    $ENV{VCPKG_ROOT}/installed/x64-linux/include
    ${CMAKE_SOURCE_DIR}/include  
    /usr/local/include  
)
