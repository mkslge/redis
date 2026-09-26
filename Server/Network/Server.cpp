#include "Network/Server.h"

#include "Protocol/ResponseFormatter.h"
#include "Networking/SocketIO.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <optional>
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

// The text protocol's session-ending commands.
bool is_quit_request(const std::string& line) {
    return line == "EXIT" || line == "exit" || line == "QUIT" || line == "quit";
}

int pending_socket_error(const int fd) {
    int error_number = 0;
    socklen_t length = sizeof(error_number);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error_number, &length) != 0) return errno;
    return error_number;
}
}

Server::Server(AofWriter& aof_writer,
               CommandProcessor& command_processor,
               StorageEngine& storage,
               const std::uint16_t port,
               const std::chrono::milliseconds expiration_sweep_interval,
               const ClientSession::Timeouts client_timeouts)
    : port_(port),
      aof_writer_(aof_writer),
      command_processor_(command_processor),
      storage_(storage),
      expiration_sweep_interval_(expiration_sweep_interval),
      client_timeouts_(client_timeouts) {
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
        const int error_number = errno;
        close(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error(socket_io::error_message(
            "bind to port " + std::to_string(port_) + " failed", error_number));
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
        // poll() array layout, which the indices below rely on:
        // [0] listening socket, [1] wakeup pipe, [2...] one entry per client.
        std::vector<pollfd> descriptors;
        descriptors.reserve(clients_.size() + 2);
        descriptors.push_back({socket_fd_, POLLIN, 0});
        descriptors.push_back({wakeup_fds_[0], POLLIN, 0});
        for (const auto& [client_fd, session] : clients_) {
            short events = POLLIN;
            if (session.has_pending_output()) events |= POLLOUT;
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
            if (events == 0) continue;

            const auto found = clients_.find(client_fd);
            if (found == clients_.end()) continue;
            ClientSession& session = found->second;

            bool keep_open = true;
            const bool has_pending_output = session.has_pending_output();
            // Flush when the socket is writable, or when it failed or hung up with
            // output still queued, so the failing send reports the error.
            if ((events & POLLOUT) || (has_pending_output && events & (POLLERR | POLLHUP))) {
                keep_open = flush_client_output(client_fd, session);
            }
            // Read when data arrived, or on an error so recv reports it.
            if (keep_open && events & (POLLIN | POLLERR)) {
                keep_open = read_from_client(client_fd, session);
            }
            // POLLNVAL: the descriptor is no longer open.
            if (events & POLLNVAL) keep_open = false;
            // The peer hung up and there is nothing left to read. Report a pending
            // socket error if there is one; otherwise close once output is flushed.
            if (keep_open && events & POLLHUP && !(events & POLLIN)) {
                const int error_number = pending_socket_error(client_fd);
                if (error_number != 0) {
                    std::cerr << socket_io::error_message(
                        "receive from client " + std::to_string(client_fd) + " failed", error_number) << std::endl;
                    keep_open = false;
                } else {
                    session.close_after_write();
                    keep_open = session.has_pending_output();
                }
            }
            if (!keep_open) close_client(client_fd);
        }

        if (std::chrono::steady_clock::now() >= next_expiration_sweep) {
            run_expiration_sweep();
            // Timeouts are checked on the same 100 ms tick, so they fire up to one
            // tick late, which is fine for limits measured in seconds.
            close_timed_out_clients();
            next_expiration_sweep = std::chrono::steady_clock::now() + expiration_sweep_interval_;
        }
    }

    // A client can finish connecting (the kernel completes the handshake) before the
    // loop accepts it. Such connections wait in the listen queue, so closing only the
    // accepted clients would leave them hanging until the Server is destroyed. Accept
    // them so they are closed like the rest, then close the listening socket so any
    // later connection attempt is reset instead of queued.
    accept_ready_clients();
    close_all_clients();
    close(socket_fd_);
    socket_fd_ = -1;
}

void Server::accept_ready_clients() {
    while (true) {
        const int client_fd = accept(socket_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            const int error_number = errno;
            if (error_number == EINTR) continue;
            if (error_number == EAGAIN || error_number == EWOULDBLOCK) return;
            std::cerr << socket_io::error_message("accept client failed", error_number) << std::endl;
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

bool Server::read_from_client(const int client_fd, ClientSession& session) {
    std::size_t bytes_this_event = 0;
    char buffer[kBufferSize];

    while (bytes_this_event < kMaxReadPerEvent) {
        const ssize_t count = recv(client_fd, buffer, sizeof(buffer), 0);
        if (count > 0) {
            bytes_this_event += static_cast<std::size_t>(count);
            if (!session.append_input(std::string_view(buffer, static_cast<std::size_t>(count)),
                                      ClientSession::Clock::now()) ||
                !process_client_input(session)) return false;
            if (session.closing()) return true;
            continue;
        }
        if (count == 0) {
            session.close_after_write();
            return session.has_pending_output();
        }
        const int error_number = errno;
        if (error_number == EINTR) continue;
        if (error_number == EAGAIN || error_number == EWOULDBLOCK) return true;
        std::cerr << socket_io::error_message(
            "receive from client " + std::to_string(client_fd) + " failed", error_number) << std::endl;
        return false;
    }
    return true;
}

bool Server::process_client_input(ClientSession& session) {
    while (const std::optional<std::string> line = session.next_line()) {
        if (is_quit_request(*line)) {
            session.discard_input();
            session.close_after_write();
            return session.queue_response("BYE\n", ClientSession::Clock::now());
        }
        if (line->empty()) continue;

        const CommandProcessResult result = [&] {
            try {
                return process_and_persist(*line);
            } catch (const std::exception& error) {
                std::cerr << "Fatal persistence error: " << error.what() << std::endl;
                std::terminate();
            }
        }();
        const std::string response = result.is_success()
            ? ResponseFormatter::format_result(result.processed_command().command,
                                               result.processed_command().execution_result)
            : ResponseFormatter::format_error(result.error_message());
        if (!session.queue_response(response, ClientSession::Clock::now())) return false;
    }
    return true;
}

bool Server::flush_client_output(const int client_fd, ClientSession& session) {
    std::size_t bytes_this_event = 0;

    while (session.has_pending_output() && bytes_this_event < kMaxWritePerEvent) {
        const std::size_t remaining_budget = kMaxWritePerEvent - bytes_this_event;
        const ssize_t count = socket_io::send_some(client_fd, session.pending_output().substr(0, remaining_budget));
        if (count > 0) {
            session.consume_output(static_cast<std::size_t>(count), ClientSession::Clock::now());
            bytes_this_event += static_cast<std::size_t>(count);
            continue;
        }
        const int error_number = count < 0 ? errno : EIO;
        if (error_number == EINTR) continue;
        if (error_number == EAGAIN || error_number == EWOULDBLOCK) return true;
        std::cerr << socket_io::error_message(
            "send to client " + std::to_string(client_fd) + " failed", error_number) << std::endl;
        return false;
    }

    if (!session.has_pending_output()) return !session.closing();
    return true;
}

CommandProcessResult Server::process_and_persist(const std::string& command) {
    CommandProcessResult result = command_processor_.process(command);
    if (result.is_success() && result.processed_command().should_log) {
        aof_writer_.append(result.processed_command().aof_record);
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

void Server::close_timed_out_clients() {
    const ClientSession::Clock::time_point now = ClientSession::Clock::now();
    std::vector<int> timed_out;
    for (const auto& [client_fd, session] : clients_) {
        if (const auto reason = session.timed_out(now, client_timeouts_)) {
            std::cerr << "Client " << client_fd << " timed out: " << *reason << std::endl;
            timed_out.push_back(client_fd);
        }
    }
    // Closed after the loop, because close_client erases from clients_.
    for (const int client_fd : timed_out) close_client(client_fd);
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
