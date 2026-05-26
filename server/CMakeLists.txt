add_executable(server
    main_server.cpp
    server_socket/src/logger.cpp
    server_socket/src/server_socket.cpp
    parser/src/lexer.cpp
    parser/src/parser.cpp
)

target_include_directories(server PRIVATE
    parser/include
    server_socket/include
)

target_link_libraries(server PRIVATE
    Boost::system
)

add_subdirectory(parser)