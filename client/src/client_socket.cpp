#include "../include/client_socket.h"

void Client::connect(const std::string& host, uint16_t port)
{
    tcp::resolver resolver(io_);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    boost::asio::connect(socket_, endpoints);
}

std::string Client::send(const std::string& query)
{
    uint32_t size = query.size();
    boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
    boost::asio::write(socket_, boost::asio::buffer(query));

    uint32_t respSize = 0;
    boost::asio::read(socket_, boost::asio::buffer(&respSize, sizeof(respSize)));

    std::string response(respSize, '\0');
    boost::asio::read(socket_, boost::asio::buffer(response));

    return response;
}