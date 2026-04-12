#include "TCP/Server.h"

#include "Protocol/ResponseFormatter.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

Server::Server(AOFLogger& logger, CommandProcessor& command_processor, const std::uint16_t port)
    : port_(port), logger_(logger), command_processor_(command_processor) {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        throw std::runtime_error("Error creating server socket");
    }

    const int enable_reuse = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Error setting socket options");
    }

    bind_and_listen();
}

Server::~Server() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
    }
}

void Server::bind_and_listen() {
    std::memset(&serveraddr_, 0, sizeof(serveraddr_));
    serveraddr_.sin_family = AF_INET;
    serveraddr_.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr_.sin_port = htons(port_);

    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&serveraddr_), sizeof(serveraddr_)) != 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Error binding to port");
    }

    sockaddr_in bound_address{};
    socklen_t bound_length = sizeof(bound_address);
    if (getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&bound_address), &bound_length) != 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Error getting bound port");
    }
    port_ = ntohs(bound_address.sin_port);

    if (listen(socket_fd_, 5) != 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Listening failed");
    }
}

void Server::run() {
    std::cout << "redisserver listening on port " << port_ << std::endl;

    while (!stopping_) {
        sockaddr_in client_address{};
        socklen_t client_length = sizeof(client_address);
        const int client_fd = accept(socket_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (client_fd < 0) {
            if (stopping_) {
                break;
            }
            std::cerr << "Failed to accept client" << std::endl;
            continue;
        }

        std::cout << "Client connected" << std::endl;
        handle_client(client_fd);
        close(client_fd);
        std::cout << "Client disconnected" << std::endl;
    }
}

bool Server::send_response(const int client_fd, const std::string& response) {
    const char* data = response.c_str();
    std::size_t bytes_remaining = response.size();

    while (bytes_remaining > 0) {
        const ssize_t bytes_sent = send(client_fd, data, bytes_remaining, 0);
        if (bytes_sent <= 0) {
            return false;
        }

        data += bytes_sent;
        bytes_remaining -= static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

void Server::handle_client(const int client_fd) const {
    char buffer[kBufferSize]{};
    std::string pending_input;

    while (true) {
        const ssize_t bytes_read = recv(client_fd, buffer, kBufferSize, 0);
        if (bytes_read <= 0) {
            return;
        }

        pending_input.append(buffer, static_cast<std::size_t>(bytes_read));

        std::size_t newline_position = pending_input.find('\n');
        while (newline_position != std::string::npos) {
            std::string command = pending_input.substr(0, newline_position);
            pending_input.erase(0, newline_position + 1);

            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }

            if (command == "EXIT" || command == "exit" || command == "QUIT" || command == "quit") {
                send_response(client_fd, "BYE\n");
                return;
            }

            if (!command.empty()) {
                const CommandProcessResult result = command_processor_.process(command);
                const std::string response = result.is_success()
                    ? ResponseFormatter::format_result(result.processed_command().statement_type,
                                                       result.processed_command().execution_result)
                    : ResponseFormatter::format_error(result.error_message());

                if (!send_response(client_fd, response)) {
                    return;
                }
                if (result.is_success() && result.processed_command().should_log) {
                    logger_.enqueue(result.processed_command().log_entry);
                }
            }

            newline_position = pending_input.find('\n');
        }
    }
}

void Server::stop() {
    stopping_ = true;
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

std::uint16_t Server::port() const {
    return port_;
}
