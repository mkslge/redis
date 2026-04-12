
#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

class Client {
    private:
        int socket_fd{};
        sockaddr_in server;
        char buffer[1024];
    public:
    explicit Client(const std::string& server_ip) {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(socket_fd == -1) {
            throw "Could not create socket";
        }

        server.sin_addr.s_addr = inet_addr(server_ip.c_str());
        server.sin_family = AF_INET;
        server.sin_port = htons(8080); //need to change this wont be true

        if(connect(socket_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
            throw "Could not connect to server";
        }

        std::cout << "Connected to server..." << std::endl;
    }

    void send_command(const std::string& command) {
        
        send(socket_fd, command.c_str(), command.size(), 0);
    }

    std::string get_response() {
        recv(socket_fd, buffer, 1024, 0);
        return std::string(buffer);
    }

};


#endif CLIENT_H