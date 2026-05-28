add_executable(server
    main_server.cpp
    server_socket/src/logger.cpp
    server_socket/src/server_socket.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}/executor/include
    ${CMAKE_CURRENT_SOURCE_DIR}/server_socket/include
)

target_link_libraries(server PRIVATE
    executor_lib
    Boost::system
)

target_compile_features(server PRIVATE cxx_std_17)
