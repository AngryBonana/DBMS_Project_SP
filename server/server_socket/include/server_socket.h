#ifndef SERVER_SOCKET_H
#define SERVER_SOCKET_H

#include <boost/asio.hpp>
#include <functional>
#include <string>
#include "logger.h"

using boost::asio::ip::tcp;

using QueryHandler = std::function<std::string(const std::string&)>;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, QueryHandler handler,
            Logger& logger, std::string clientId);

    void start();

private:
    void readRequest();
    void readBody();
    void sendResponse(const std::string& response);

    tcp::socket socket_;
    QueryHandler handler_;
    Logger& logger_;
    std::string clientId_;

    uint32_t bodySize_ = 0;
    std::string body_;
    uint32_t outSize_  = 0;
    std::string outBody_;
};

class Server {
public:
    Server(boost::asio::io_context& io, uint16_t port,
           QueryHandler handler, Logger& logger);

private:
    void accept();

    tcp::acceptor acceptor_;
    QueryHandler handler_;
    Logger& logger_;
    uint32_t nextClientId_ = 1;
};

#endif // SERVER_SOCKET_H