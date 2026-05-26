#include "include/client_socket.h"
#include <iostream>
#include <string>
#include <fstream>

void print_help(const std::string& name_of_prog)
{
    std::cout << "Usage:" << std::endl;
    std::cout << "Interactive mode: " <<
    name_of_prog << " <port_number>" << std::endl;
    std::cout << "Script mode: " <<
    name_of_prog << " <port_number> <script>.txt" << std::endl;
}

std::string readQuery() {
    std::string result = "";
    int c;
    while ((c = std::cin.get()) != EOF)
    {
        result += static_cast<char>(c);
        if (c == ';') break;
    }
    return result;
}

std::string readQueryFromFile(std::ifstream& file)
{
    std::string result = "";
    int c;
    while ((c = file.get()) != EOF)
    {
        result += static_cast<char>(c);
        if (c == ';') break;
    }
    return result;
}

int main(int argc, char* argv[])
{
    if (argc == 2 || argc == 3)
    {
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

        Client client;
        try {
            client.connect("127.0.0.1", port);
        }
        catch (const boost::system::system_error& e) {
            std::cerr << "Failed to connect to server at port "
                      << port << ": " << e.what() << std::endl;
            return 1;
        }

        if (argc == 2)
        {
            while (std::cin)
            {
                std::cout << "> ";
                std::string cmd = readQuery();
                if (cmd.empty()) continue;

                try {
                    std::cout << client.send(cmd) << std::endl;
                }
                catch (const boost::system::system_error& e) {
                    std::cerr << "Connection lost: " << e.what() << std::endl;
                    return 1;
                }
            }
        }
        else
        {
            std::ifstream file;
            file.open(argv[2]);
            if (!file.is_open()) {
                std::cerr << "Can't open file: " << argv[2] << std::endl;
                return 1;
            }
            while (true)
            {
                std::string cmd = readQueryFromFile(file);
                if (cmd.empty()) break;

                try {
                    std::cout << client.send(cmd) << std::endl;
                }
                catch (const boost::system::system_error& e) {
                    std::cerr << "Connection lost: " << e.what() << std::endl;
                    return 1;
                }
            }

        }
    }
    else
    {
        print_help(argv[0]);
    }
    return 0;
}