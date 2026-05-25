#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <boost/asio.hpp>
#include <string>

using boost::asio::ip::tcp;

class Client {
public:
    Client() : socket_(io_) {}

    void connect(const std::string& host, uint16_t port);
    std::string send(const std::string& query);

private:
    boost::asio::io_context io_;
    tcp::socket socket_;
};

#endif //CLIENT_SOCKET_H