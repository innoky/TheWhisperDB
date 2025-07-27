cmake_minimum_required(VERSION 3.14)
project(CrowExample)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Установка пути к vcpkg, если нужно
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

add_executable(server src/main.cpp)

# Указываем путь к Crow через vcpkg
target_include_directories(server PRIVATE
    $ENV{VCPKG_ROOT}/installed/x64-linux/include
)

# Crow не требует отдельной линковки, но нужен pthread
target_link_libraries(server PRIVATE pthread)
