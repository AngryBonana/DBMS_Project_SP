cmake_minimum_required(VERSION 3.16)

# Подмодуль executor: библиотека + тесты этапов (подключается из корневого CMakeLists.txt).
project(executor_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(executor_lib STATIC
    src/request_types.cpp
    src/query_classifier.cpp
    src/json_response.cpp
    src/access_logger.cpp
    src/async_executor.cpp
    src/executor_service.cpp
)

target_include_directories(executor_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(executor_lib PUBLIC cxx_std_17)

# Тесты: при сборке из корня googletest уже подключён через FetchContent.
if(TARGET GTest::gtest)
    add_executable(executor_tests
        tests/test_stage1.cpp
        tests/test_stage2.cpp
    )

    target_link_libraries(executor_tests PRIVATE
        executor_lib
        GTest::gtest
        GTest::gtest_main
    )

    target_compile_features(executor_tests PRIVATE cxx_std_17)

    add_test(NAME ExecutorTests COMMAND executor_tests)
endif()

# Автономная сборка только executor: cmake -S executor -B executor/build
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND NOT TARGET GTest::gtest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(executor_tests
        tests/test_stage1.cpp
        tests/test_stage2.cpp
    )

    target_link_libraries(executor_tests PRIVATE
        executor_lib
        GTest::gtest
        GTest::gtest_main
    )

    add_test(NAME ExecutorTests COMMAND executor_tests)
endif()
