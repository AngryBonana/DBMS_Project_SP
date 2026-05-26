#include "../include/server_socket.h"


Session::Session(tcp::socket socket, QueryHandler handler,
    Logger& logger, std::string clientId)
    : socket_(std::move(socket)),
    handler_(std::move(handler)),
    logger_(logger),
    clientId_(std::move(clientId))
{}

void Session::start()
{
    readRequest();
}

void Session::readRequest()
{
    auto self = shared_from_this();

    boost::asio::async_read(
        socket_,
        boost::asio::buffer(&bodySize_, sizeof(bodySize_)),
        [this, self](boost::system::error_code ec, size_t)
        {
            if (ec)
            {
                logger_.logDisconnect(clientId_);
                return;
            }
            body_.resize(bodySize_);
            readBody();
        }
    );
}

void Session::readBody()
{
    auto self = shared_from_this();

    boost::asio::async_read(
        socket_,
        boost::asio::buffer(body_),
        [this, self](boost::system::error_code ec, size_t)
        {
            if (ec)
            {
                logger_.logDisconnect(clientId_);
                return;
            }

            auto startTime = std::chrono::system_clock::now();

            std::string handlerId = clientId_ + "_" +
                std::to_string(startTime.time_since_epoch().count());

            std::string response;
            int statusCode = 0;
            std::string statusMsg  = "OK";

            try
            {
                response = handler_(body_);
            }
            catch (const std::exception& e)
            {
                statusCode = 1;
                statusMsg = e.what();
                response = "ERROR: " + statusMsg;
            }

            auto endTime = std::chrono::system_clock::now();

            logger_.logRequest({
                clientId_,
                handlerId,
                body_,
                startTime,
                endTime,
                statusCode,
                statusMsg
            });

            sendResponse(response);
        }
    );
}

void Session::sendResponse(const std::string& response)
{
    auto self = shared_from_this();

    outSize_ = response.size();
    outBody_ = response;

    std::vector<boost::asio::const_buffer> buffers = {
        boost::asio::buffer(&outSize_, sizeof(outSize_)),
        boost::asio::buffer(outBody_)
    };

    boost::asio::async_write(
        socket_, buffers,
        [this, self](boost::system::error_code ec, size_t)
        {
            if (!ec) readRequest();
            else logger_.logDisconnect(clientId_);
        }
    );
}


Server::Server(boost::asio::io_context& io, uint16_t port,
               QueryHandler handler, Logger& logger)
    : acceptor_(io, tcp::endpoint(tcp::v4(), port)),
    handler_(std::move(handler)),
    logger_(logger)
{
    accept();
}

void Server::accept()
{
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::string clientId = "client_" + std::to_string(nextClientId_++);
                logger_.logConnect(clientId);
                std::make_shared<Session>(
                    std::move(socket), handler_, logger_, clientId
                )->start();
            }
            accept();
        }
    );
}