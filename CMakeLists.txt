cmake_minimum_required(VERSION 3.16)

project(DBMS_Project_SP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


enable_testing()

message([[VCPKG toolchain setup example. It is needed for some libraries. Highly recommended to pass it by command line arguments for cmake configuration (IDE can help with this).
        set(VCPKG_TARGET_TRIPLET x64-windows)
        set(CMAKE_TOOLCHAIN_FILE C:/Programming/vcpkg/scripts/buildsystems/vcpkg.cmake)]])

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip)
FetchContent_MakeAvailable(googletest)

find_package(Boost COMPONENTS system container REQUIRED) 

add_subdirectory(client)
add_subdirectory(index)
add_subdirectory(storage)
add_subdirectory(server/parser)
add_subdirectory(executor)
add_subdirectory(server)
# add_subdirectory(common)