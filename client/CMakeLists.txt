add_executable(client
    main_client.cpp
    src/client_socket.cpp
)

target_include_directories(client PRIVATE include)

target_link_libraries(client PRIVATE
    Boost::system
)