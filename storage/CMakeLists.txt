cmake_minimum_required(VERSION 3.16)

# Сборка из storage/build (WSL и т.п.) — отдельный проект с GTest.
# Из корня репозитория googletest уже подключён в корневом CMakeLists.txt.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(storage LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )
    # Для FetchContent в WSL/Linux
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_library(storage_core
    src/value.cpp
    src/string_pool.cpp
    src/table.cpp
    src/database.cpp
    src/dbms.cpp
    src/bstarplus_adapter.cpp
)

target_include_directories(storage_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(storage_core PUBLIC
    btree_lib
)

target_compile_features(storage_core PUBLIC cxx_std_17)

add_executable(storage_tests
    tests/core_test.cpp
    tests/table_test.cpp
    tests/integration_test.cpp
)

target_include_directories(storage_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(storage_tests PRIVATE
    storage_core
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_compile_features(storage_tests PRIVATE cxx_std_17)

add_test(NAME StorageTests COMMAND $<TARGET_FILE:storage_tests>)
