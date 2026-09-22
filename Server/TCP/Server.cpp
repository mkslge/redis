#include "Server.h"

#include "ResponseFormatter.h"
#include "SocketIO.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {
bool set_nonblocking(const int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool set_close_on_exec(const int fd) {
    const int flags = fcntl(fd, F_GETFD, 0);
    return flags >= 0 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}
}

Server::Server(AOFLogger& logger,
               CommandProcessor& command_processor,
               StorageEngine& storage,
               const std::uint16_t port,
               const std::chrono::milliseconds expiration_sweep_interval)
    : port_(port),
      logger_(logger),
      command_processor_(command_processor),
      storage_(storage),
      expiration_sweep_interval_(expiration_sweep_interval) {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1) throw std::runtime_error("Error creating server socket");

    const int enable_reuse = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)) < 0 ||
        !set_nonblocking(socket_fd_) || !set_close_on_exec(socket_fd_)) {
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Error configuring server socket");
    }

    bind_and_listen();
    initialize_wakeup_pipe();
}

Server::~Server() {
    stop();
    close_all_clients();
    if (socket_fd_ >= 0) close(socket_fd_);
    if (wakeup_fds_[0] >= 0) close(wakeup_fds_[0]);
    if (wakeup_fds_[1] >= 0) close(wakeup_fds_[1]);
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

    if (listen(socket_fd_, 128) != 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Listening failed");
    }
}

void Server::initialize_wakeup_pipe() {
    if (pipe(wakeup_fds_) != 0 ||
        !set_nonblocking(wakeup_fds_[0]) || !set_nonblocking(wakeup_fds_[1]) ||
        !set_close_on_exec(wakeup_fds_[0]) || !set_close_on_exec(wakeup_fds_[1])) {
        if (wakeup_fds_[0] >= 0) close(wakeup_fds_[0]);
        if (wakeup_fds_[1] >= 0) close(wakeup_fds_[1]);
        wakeup_fds_[0] = wakeup_fds_[1] = -1;
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Error creating server wakeup pipe");
    }
}

void Server::run() {
    std::cout << "redisserver listening on port " << port_ << std::endl;
    auto next_expiration_sweep = std::chrono::steady_clock::now() + expiration_sweep_interval_;

    while (!stopping_) {
        std::vector<pollfd> descriptors;
        descriptors.reserve(clients_.size() + 2);
        descriptors.push_back({socket_fd_, POLLIN, 0});
        descriptors.push_back({wakeup_fds_[0], POLLIN, 0});
        for (const auto& [client_fd, session] : clients_) {
            short events = POLLIN;
            if (session.output_offset < session.output_buffer.size()) events |= POLLOUT;
            descriptors.push_back({client_fd, events, 0});
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            next_expiration_sweep - std::chrono::steady_clock::now());
        const int timeout = static_cast<int>(std::max<std::int64_t>(0, remaining.count()));
        const int ready = poll(descriptors.data(), descriptors.size(), timeout);
        if (ready < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("Event polling failed: " + std::string(std::strerror(errno)));
        }

        if (descriptors[1].revents & POLLIN) drain_wakeup_pipe();
        if (stopping_) break;
        if (descriptors[0].revents & POLLIN) accept_ready_clients();

        for (std::size_t index = 2; index < descriptors.size(); ++index) {
            const int client_fd = descriptors[index].fd;
            const short events = descriptors[index].revents;
            if (events == 0 || !clients_.contains(client_fd)) continue;

            bool keep_open = true;
            if (events & POLLIN) keep_open = read_from_client(client_fd);
            if (keep_open && clients_.contains(client_fd) && events & POLLOUT) {
                keep_open = flush_client_output(client_fd);
            }
            if (events & (POLLERR | POLLNVAL)) keep_open = false;
            if (keep_open && events & POLLHUP && !(events & POLLIN)) {
                ClientSession& session = clients_.at(client_fd);
                session.close_after_write = true;
                keep_open = session.output_offset < session.output_buffer.size();
            }
            if (!keep_open) close_client(client_fd);
        }

        if (std::chrono::steady_clock::now() >= next_expiration_sweep) {
            run_expiration_sweep();
            next_expiration_sweep = std::chrono::steady_clock::now() + expiration_sweep_interval_;
        }
    }

    close_all_clients();
}

void Server::accept_ready_clients() {
    while (true) {
        const int client_fd = accept(socket_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            std::cerr << "Failed to accept client: " << std::strerror(errno) << std::endl;
            return;
        }

        if (!set_nonblocking(client_fd) || !set_close_on_exec(client_fd) ||
            !socket_io::configure_for_writes(client_fd)) {
            close(client_fd);
            continue;
        }

        clients_.try_emplace(client_fd);
        std::cout << "Client " << client_fd << " connected" << std::endl;
    }
}

bool Server::read_from_client(const int client_fd) {
    auto found = clients_.find(client_fd);
    if (found == clients_.end()) return false;
    ClientSession& session = found->second;
    std::size_t bytes_this_event = 0;
    char buffer[kBufferSize];

    while (bytes_this_event < kMaxReadPerEvent) {
        const ssize_t count = recv(client_fd, buffer, sizeof(buffer), 0);
        if (count > 0) {
            bytes_this_event += static_cast<std::size_t>(count);
            session.input_buffer.append(buffer, static_cast<std::size_t>(count));
            if (session.input_buffer.size() > kMaxInputBuffer || !process_client_input(client_fd)) return false;
            if (session.close_after_write) return true;
            continue;
        }
        if (count == 0) {
            session.close_after_write = true;
            return session.output_offset < session.output_buffer.size();
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
        return false;
    }
    return true;
}

bool Server::process_client_input(const int client_fd) {
    ClientSession& session = clients_.at(client_fd);
    std::size_t newline_position = session.input_buffer.find('\n');
    while (newline_position != std::string::npos) {
        std::string command = session.input_buffer.substr(0, newline_position);
        session.input_buffer.erase(0, newline_position + 1);
        if (!command.empty() && command.back() == '\r') command.pop_back();

        if (command == "EXIT" || command == "exit" || command == "QUIT" || command == "quit") {
            session.input_buffer.clear();
            session.close_after_write = true;
            return queue_response(session, "BYE\n");
        }

        if (!command.empty()) {
            const CommandProcessResult result = [&] {
                try {
                    return process_and_persist(command);
                } catch (const std::exception& error) {
                    std::cerr << "Fatal persistence error: " << error.what() << std::endl;
                    std::terminate();
                }
            }();
            std::string response = result.is_success()
                ? ResponseFormatter::format_result(result.processed_command().command,
                                                   result.processed_command().execution_result)
                : ResponseFormatter::format_error(result.error_message());
            if (!queue_response(session, std::move(response))) return false;
        }

        newline_position = session.input_buffer.find('\n');
    }
    return true;
}

bool Server::queue_response(ClientSession& session, std::string response) {
    const std::size_t pending = session.output_buffer.size() - session.output_offset;
    if (response.size() > kMaxOutputBuffer - pending) return false;
    if (session.output_offset > 0) {
        session.output_buffer.erase(0, session.output_offset);
        session.output_offset = 0;
    }
    session.output_buffer += response;
    return true;
}

bool Server::flush_client_output(const int client_fd) {
    ClientSession& session = clients_.at(client_fd);
    std::size_t bytes_this_event = 0;

    while (session.output_offset < session.output_buffer.size() && bytes_this_event < kMaxWritePerEvent) {
        const std::size_t remaining_budget = kMaxWritePerEvent - bytes_this_event;
        const std::string_view pending(session.output_buffer.data() + session.output_offset,
                                       session.output_buffer.size() - session.output_offset);
        const ssize_t count = socket_io::send_some(client_fd, pending.substr(0, remaining_budget));
        if (count > 0) {
            session.output_offset += static_cast<std::size_t>(count);
            bytes_this_event += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        return false;
    }

    if (session.output_offset == session.output_buffer.size()) {
        session.output_buffer.clear();
        session.output_offset = 0;
        return !session.close_after_write;
    }
    return true;
}

CommandProcessResult Server::process_and_persist(const std::string& command) {
    CommandProcessResult result = command_processor_.process(command);
    if (result.is_success() && result.processed_command().should_log) {
        logger_.append_record(result.processed_command().aof_record);
    }
    return result;
}

void Server::close_client(const int client_fd) {
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    clients_.erase(client_fd);
    std::cout << "Client " << client_fd << " disconnected" << std::endl;
}

void Server::close_all_clients() {
    for (const auto& [client_fd, session] : clients_) {
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
    }
    clients_.clear();
}

void Server::drain_wakeup_pipe() const {
    char buffer[64];
    while (read(wakeup_fds_[0], buffer, sizeof(buffer)) > 0) {}
}

void Server::run_expiration_sweep() {
    storage_.prune_expired_batch(kExpirationCandidatesPerSweep, StorageEngine::Clock::now());
}

void Server::stop() {
    stopping_ = true;
    if (wakeup_fds_[1] < 0) return;
    const char byte = 1;
    while (write(wakeup_fds_[1], &byte, 1) < 0 && errno == EINTR) {}
}

std::uint16_t Server::port() const {
    return port_;
}
