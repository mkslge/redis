#include "Networking/Client.h"

#include <iostream>
#include <stdexcept>

Client::Client(const std::string& server_ip, const std::uint16_t port)
    : server(build_server_address(server_ip, port)) {
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        throw std::runtime_error("Could not create socket");
    }

    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
        close(socket_fd);
        socket_fd = -1;
        throw std::runtime_error("Could not connect to server");
    }

    std::cout << "Connected to server..." << std::endl;
}

Client::~Client() {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

sockaddr_in Client::build_server_address(const std::string& server_ip, const std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip.c_str(), &address.sin_addr) != 1) {
        throw std::invalid_argument("Invalid IPv4 address");
    }

    return address;
}

std::string Client::response_from_buffer(const char* response_buffer, const std::size_t bytes_read) {
    return std::string(response_buffer, bytes_read);
}

void Client::send_command(const std::string& command) {
    std::string framed_command = command;
    if (framed_command.empty() || framed_command.back() != '\n') {
        framed_command.push_back('\n');
    }

    send(socket_fd, framed_command.c_str(), framed_command.size(), 0);
}

std::string Client::get_response() {
    std::size_t newline_position = pending_response_.find('\n');
    while (newline_position == std::string::npos) {
        const ssize_t bytes_read = recv(socket_fd, buffer, kBufferSize, 0);
        if (bytes_read <= 0) {
            const std::string remaining_response = pending_response_;
            pending_response_.clear();
            return remaining_response;
        }

        pending_response_ += response_from_buffer(buffer, static_cast<std::size_t>(bytes_read));
        newline_position = pending_response_.find('\n');
    }

    std::string response = pending_response_.substr(0, newline_position);
    pending_response_.erase(0, newline_position + 1);
    return response;
}
