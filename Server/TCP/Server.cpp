#include "TCP/Server.h"

#include "Protocol/ResponseFormatter.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

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
    shutdown_clients();
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
        reap_finished_clients();

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

        //a client can disconnect while the accept loop is blocked, and the OS may
        //reuse that fd for the next connection before we can reap the old entry.
        reap_finished_clients();

        bool inserted = false;
        {
            std::lock_guard<std::mutex> lock{mutex_};
            inserted = clients_.try_emplace(client_fd, ClientSession{}).second;
        }

        if (!inserted) {
            std::cerr << "Client fd " << client_fd << " was reused before the old session was reaped" << std::endl;
            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
            continue;
        }

        std::thread worker(&Server::handle_client, this, client_fd);
        {
            std::lock_guard<std::mutex> lock{mutex_};
            auto it = clients_.find(client_fd);
            if (it != clients_.end()) {
                it->second.worker = std::move(worker);
                it->second.finished = false;
            }
        }

        std::cout << "Client " << client_fd << " connected" << std::endl;
    }

    shutdown_clients();
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

void Server::handle_client(const int client_fd) {
    char buffer[kBufferSize]{};
    std::string pending_input;
    bool should_send_bye = false;

    while (true) {
        const ssize_t bytes_read = recv(client_fd, buffer, kBufferSize, 0);
        if (bytes_read <= 0) {
            break;
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
                should_send_bye = true;
                break;
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

        if (should_send_bye) {
            break;
        }
    }

    if (should_send_bye) {
        send_response(client_fd, "BYE\n");
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    mark_client_finished(client_fd);
}

void Server::mark_client_finished(const int client_fd) {
    std::lock_guard<std::mutex> lock{mutex_};
    auto it = clients_.find(client_fd);
    if (it != clients_.end()) {
        it->second.finished = true;
    }
}

void Server::reap_finished_clients() {
    std::vector<std::thread> threads_to_join;

    {
        std::lock_guard<std::mutex> lock{mutex_};
        for (auto it = clients_.begin(); it != clients_.end();) {
            if (!it->second.finished) {
                ++it;
                continue;
            }

            threads_to_join.push_back(std::move(it->second.worker));
            std::cout << "Client " << it->first << " disconnected" << std::endl;
            it = clients_.erase(it);
        }
    }

    for (auto& thread : threads_to_join) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void Server::shutdown_clients() {
    std::vector<int> client_fds;
    std::vector<std::thread> threads_to_join;

    {
        std::lock_guard<std::mutex> lock{mutex_};
        client_fds.reserve(clients_.size());
        threads_to_join.reserve(clients_.size());

        for (auto& [client_fd, session] : clients_) {
            client_fds.push_back(client_fd);
            threads_to_join.push_back(std::move(session.worker));
        }
        clients_.clear();
    }

    for (const int client_fd : client_fds) {
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
    }

    for (auto& thread : threads_to_join) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void Server::stop() {
    stopping_ = true;
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    shutdown_clients();
}

std::uint16_t Server::port() const {
    return port_;
}
