

#include <iostream>
#include <cstdint>
#include <exception>
#include <string>

#include "Networking/Client.h"

int main(int argc, char *argv[]) {
    if(argc != 2 && argc != 3) {
        std::cout << "Error, expected format ./redisclient {SERVER_IP} [PORT]" << std::endl;

        exit(1);
    } 
    const std::string server_ip(argv[1]);
    const std::uint16_t port = argc == 3
        ? static_cast<std::uint16_t>(std::stoi(argv[2]))
        : Client::kDefaultPort;

    try {
        Client client(server_ip, port);

        std::cout << "redisclient connected to " << server_ip << ":" << port << std::endl;
        std::cout << "Type commands. Use QUIT to close the server session." << std::endl;

        std::string line;
        while (true) {
            std::cout << "> " << std::flush;
            if (!std::getline(std::cin, line)) {
                break;
            }

            if (line.empty()) {
                continue;
            }

            client.send_command(line);
            std::string response = client.get_response();
            std::cout << response << std::endl;
            if(response == "BYE") {
                break;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}
