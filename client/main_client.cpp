#include "include/client_protocol.h"
#include "include/client_socket.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

std::string trimQuery(std::string query) {
    while (!query.empty() &&
           (query.back() == '\r' || query.back() == '\n' || query.back() == ' ' ||
            query.back() == '\t')) {
        query.pop_back();
    }
    return query;
}

bool isIgnorableQuery(const std::string& query) {
    for (char ch : query) {
        if (!std::isspace(static_cast<unsigned char>(ch)) && ch != ';') {
            return false;
        }
    }
    return true;
}

void print_help(const std::string& name_of_prog)
{
    std::cout << "Usage:" << std::endl;
    std::cout << "Interactive mode: " << name_of_prog << " <port_number>" << std::endl;
    std::cout << "Script mode: " << name_of_prog << " <port_number> <script>.txt" << std::endl;
    std::cout << "Server responses are JSON. Async DDL/DML are awaited automatically." << std::endl;
}

std::string readQuery() {
    std::string result;
    int c = 0;
    while ((c = std::cin.get()) != EOF) {
        result += static_cast<char>(c);
        if (c == ';') {
            break;
        }
    }
    return result;
}

std::string readQueryFromFile(std::ifstream& file) {
    std::string result;
    int c = 0;
    while ((c = file.get()) != EOF) {
        result += static_cast<char>(c);
        if (c == ';') {
            break;
        }
    }
    return result;
}

int main(int argc, char* argv[])
{
    if (argc == 2 || argc == 3) {
        int port = 0;
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception&) {
            std::cerr << "Invalid port: " << argv[1] << std::endl;
            return 1;
        }

        if (port < 0 || port > 65535) {
            std::cerr << "Invalid port number" << std::endl;
            return 1;
        }

        Client client;
        try {
            client.connect("127.0.0.1", port);
        } catch (const boost::system::system_error& e) {
            std::cerr << "Failed to connect to server at port " << port << ": " << e.what()
                      << std::endl;
            return 1;
        }

        if (argc == 2) {
            while (std::cin) {
                std::cout << "> ";
                std::string cmd = trimQuery(readQuery());
                if (cmd.empty() || isIgnorableQuery(cmd)) {
                    continue;
                }

                try {
                    std::cout << client_protocol::sendAndWait(client, cmd) << std::endl;
                } catch (const boost::system::system_error& e) {
                    std::cerr << "Connection lost: " << e.what() << std::endl;
                    return 1;
                }
            }
        } else {
            std::ifstream file(argv[2]);
            if (!file.is_open()) {
                std::cerr << "Can't open file: " << argv[2] << std::endl;
                return 1;
            }
            while (true) {
                std::string cmd = trimQuery(readQueryFromFile(file));
                if (cmd.empty()) {
                    break;
                }
                if (isIgnorableQuery(cmd)) {
                    continue;
                }

                try {
                    std::cout << client_protocol::sendAndWait(client, cmd) << std::endl;
                } catch (const boost::system::system_error& e) {
                    std::cerr << "Connection lost: " << e.what() << std::endl;
                    return 1;
                }
            }
        }
    } else {
        print_help(argv[0]);
    }
    return 0;
}
