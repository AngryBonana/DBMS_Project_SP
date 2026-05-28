cmake_minimum_required(VERSION 3.16)

project(executor_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(executor_lib STATIC
    src/request_types.cpp
    src/access_logger.cpp
    src/async_executor.cpp
)

target_include_directories(executor_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(executor_lib PUBLIC cxx_std_17)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(executor_stage1_tests
        tests/test_stage1.cpp
    )

    target_link_libraries(executor_stage1_tests PRIVATE
        executor_lib
        GTest::gtest
        GTest::gtest_main
    )

    target_compile_features(executor_stage1_tests PRIVATE cxx_std_17)

    add_test(NAME ExecutorStage1Tests COMMAND executor_stage1_tests)
endif()
