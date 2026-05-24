add_library(parser_lib STATIC
    src/lexer.cpp
    src/parser.cpp
)

target_include_directories(parser_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(parser_lib PUBLIC
    Boost::system
    Boost::container
)

target_compile_features(parser_lib PUBLIC cxx_std_17)

add_subdirectory(tests)