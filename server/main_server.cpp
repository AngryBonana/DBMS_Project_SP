#include "executor.h"
#include "server_socket/include/logger.h"
#include "server_socket/include/server_socket.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

void print_help(const std::string& prog_name)
{
    std::cout << "Usage: " << prog_name << " <port> [data_dir]\n"
              << "  data_dir defaults to ./data\n";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3)
    {
        print_help(argv[0]);
        return 0;
    }

    int port = 0;
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

    const std::filesystem::path dataRoot =
        (argc == 3) ? std::filesystem::path(argv[2]) : std::filesystem::path("data");
    std::filesystem::create_directories(dataRoot);

    Logger logger("db_log.txt");

    // Точка входа сервера: сеть → ExecutorService → DbmsQueryEngine (без parse в main).
    auto engine = std::make_shared<executor::DbmsQueryEngine>(dataRoot);
    executor::ExecutorConfig config;
    config.accessLogPath = (dataRoot / "access.log").string();

    auto service = std::make_shared<executor::ExecutorService>(
        [engine](const std::string& query) { return engine->execute(query); },
        config);

    QueryHandler handler = [service](const std::string& query,
                                     const std::string& clientId) -> std::string {
        return service->handleClientCommand(query, clientId);
    };

    try {
        boost::asio::io_context io;
        Server server(io, static_cast<uint16_t>(port), handler, logger);

        std::cout << "Server started on port " << port
                  << " (data: " << dataRoot.string() << ")\n";
        io.run();
    }
    catch (const boost::system::system_error& e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
