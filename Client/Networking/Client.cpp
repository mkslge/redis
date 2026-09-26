#include "Client.h"
#include "Networking/SocketIO.h"

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <iostream>
#include <stdexcept>

namespace {
timeval to_timeval(const std::chrono::milliseconds duration) {
    return timeval{
        .tv_sec = static_cast<time_t>(duration.count() / 1000),
        .tv_usec = static_cast<suseconds_t>((duration.count() % 1000) * 1000)};
}
}

Client::Client(const std::string& server_ip,
               const std::uint16_t port,
               const std::chrono::milliseconds connect_timeout,
               const std::chrono::milliseconds response_timeout)
    : server(build_server_address(server_ip, port)) {
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        throw std::runtime_error("Could not create socket");
    }

    if (!socket_io::configure_for_writes(socket_fd)) {
        close(socket_fd);
        socket_fd = -1;
        throw std::runtime_error("Could not configure socket");
    }

    // Blocking send and recv give up after response_timeout instead of waiting forever.
    const timeval io_timeout = to_timeval(response_timeout);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &io_timeout, sizeof(io_timeout)) != 0 ||
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &io_timeout, sizeof(io_timeout)) != 0) {
        close(socket_fd);
        socket_fd = -1;
        throw std::runtime_error("Could not configure socket timeouts");
    }

    connect_with_timeout(server_ip, port, connect_timeout);

    std::cout << "Connected to server..." << std::endl;
}

void Client::connect_with_timeout(const std::string& server_ip, const std::uint16_t port,
                                  const std::chrono::milliseconds timeout) {
    const std::string target = server_ip + ":" + std::to_string(port);
    const auto fail = [&](const std::string& message) {
        close(socket_fd);
        socket_fd = -1;
        throw std::runtime_error(message);
    };

    // A blocking connect() can wait over a minute on an unreachable host. Instead,
    // start it nonblocking, wait up to `timeout` for it to finish, then switch the
    // socket back to blocking for normal use.
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        fail("Could not configure socket");
    }

    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
        // EINPROGRESS means the handshake started and will finish later.
        if (errno != EINPROGRESS) {
            fail(socket_io::error_message("connect to " + target + " failed", errno));
        }
        pollfd pending{.fd = socket_fd, .events = POLLOUT, .revents = 0};
        int ready = 0;
        do {
            ready = poll(&pending, 1, static_cast<int>(timeout.count()));
        } while (ready < 0 && errno == EINTR);
        if (ready == 0) {
            fail("connect to " + target + " timed out after " +
                 std::to_string(timeout.count()) + " ms");
        }
        // The socket is writable once the handshake finished, successfully or not;
        // SO_ERROR says which.
        int error_number = ready < 0 ? errno : 0;
        socklen_t length = sizeof(error_number);
        if (ready > 0 && getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error_number, &length) != 0) {
            error_number = errno;
        }
        if (error_number != 0) {
            fail(socket_io::error_message("connect to " + target + " failed", error_number));
        }
    }

    if (fcntl(socket_fd, F_SETFL, flags) != 0) fail("Could not configure socket");
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

bool Client::send_command(const std::string& command) {
    std::string framed_command = command;
    if (framed_command.empty() || framed_command.back() != '\n') {
        framed_command.push_back('\n');
    }

    if (!socket_io::send_all(socket_fd, framed_command)) {
        const int error_number = errno;
        throw std::runtime_error(socket_io::error_message("send command failed", error_number));
    }
    return true;
}

std::string Client::get_response() {
    std::size_t newline_position = pending_response_.find('\n');
    while (newline_position == std::string::npos) {
        const ssize_t bytes_read = recv(socket_fd, buffer, kBufferSize, 0);
        if (bytes_read > 0) {
            pending_response_ += response_from_buffer(buffer, static_cast<std::size_t>(bytes_read));
            newline_position = pending_response_.find('\n');
            continue;
        }
        if (bytes_read == 0) {
            const std::string remaining_response = pending_response_;
            pending_response_.clear();
            return remaining_response;
        }

        const int error_number = errno;
        if (error_number == EINTR) continue;
        // SO_RCVTIMEO expired: the server sent nothing for the whole response timeout.
        if (error_number == EAGAIN || error_number == EWOULDBLOCK) {
            throw std::runtime_error("receive response failed: timed out waiting for the server");
        }
        throw std::runtime_error(socket_io::error_message("receive response failed", error_number));
    }

    std::string response = pending_response_.substr(0, newline_position);
    pending_response_.erase(0, newline_position + 1);  // + 1 also removes the '\n' itself.
    return response;
}
