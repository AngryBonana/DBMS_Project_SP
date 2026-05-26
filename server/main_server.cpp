#include "parser/include/lexer.h"
#include "parser/include/parser.h"
#include "parser/include/parse_error.h"
#include "server_socket/include/logger.h"
#include "server_socket/include/server_socket.h"
#include <string>
#include <iostream>

void print_help(const std::string& prog_name)
{
    std::cout << "Using: " << prog_name << " <port>" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        print_help(argv[0]);
        return 0;
    }

    int port;
    try {
        port = std::stoi(argv[1]);
    }
    catch (const std::exception&)
    {
        std::cerr << "Invalid port: " << argv[1] << std::endl;
        return 1;
    }

    if (port < 0 || port > 65535)
    {
        std::cerr << "Invalid port number" << std::endl;
        return 1;
    }

    Logger logger("db_log.txt");

    QueryHandler handler = [&](const std::string& query) -> std::string {
        try {
            auto tokens = Lexer(query).tokenize();
            auto cmd    = Parser(std::move(tokens)).parse();
            // return std::visit(executor, cmd);
            return "OK"; // заглушка пока нет executor
        }
        catch (const std::exception& e) {
            return std::string("ERROR: ") + e.what();
        }
    };

    try {
        boost::asio::io_context io;
        Server server(io, port, handler, logger);

        std::cout << "Server started on port " << port << std::endl;
        io.run();
    }
    catch (const boost::system::system_error& e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}