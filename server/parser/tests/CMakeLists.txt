add_executable(parser_tests
    main_test.cpp
    lexer_test.cpp
    parser_test.cpp
)

target_include_directories(parser_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/server/parser/include
)

target_link_libraries(parser_tests PRIVATE
    parser_lib
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
    Boost::system
    Boost::container
)

target_compile_features(parser_tests PRIVATE cxx_std_17)

add_test(NAME ParserTests COMMAND $<TARGET_FILE:parser_tests>)

set_tests_properties(ParserTests PROPERTIES
    TIMEOUT 30
    LABELS "parser"
)