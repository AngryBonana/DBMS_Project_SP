cmake_minimum_required(VERSION 3.16)

add_library(storage_core
    src/value.cpp
    src/string_pool.cpp
    src/table.cpp
    src/database.cpp
    src/dbms.cpp
)

target_include_directories(storage_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(storage_core PUBLIC
    # не требуется Boost для core
)

target_compile_features(storage_core PUBLIC cxx_std_17)

add_executable(storage_tests
    tests/main_test.cpp
    tests/value_test.cpp
    tests/string_pool_test.cpp
    tests/schema_test.cpp
    tests/table_test.cpp
    tests/table_persist_test.cpp
    tests/database_test.cpp
    tests/dbms_integration_test.cpp
    tests/index_hook_test.cpp
)

target_include_directories(storage_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(storage_tests PRIVATE
    storage_core
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_compile_features(storage_tests PRIVATE cxx_std_17)

add_test(NAME StorageTests COMMAND storage_tests)
