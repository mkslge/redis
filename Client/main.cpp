

#include <iostream>

#include "Networking/Client.h"

int main(int argc, char *argv[]) {
    if(argc != 2) {
        std::cout << "Error, expected format ./client {SERVER_IP}" << std::endl;

        exit(1);
    } 
    std::string str(argv[1]);
    Client client(str);
    std::string line;
    while(std::getline(std::cin, line)) {
        client.send_command(line);
        client.get_response();
    }

    return 0;
}