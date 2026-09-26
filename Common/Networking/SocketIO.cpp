#include "Networking/SocketIO.h"

#include <cerrno>
#include <sys/socket.h>
#include <system_error>

namespace {
int send_flags() {
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}
}

std::string socket_io::error_message(const std::string_view operation, const int error_number) {
    const std::error_code error(error_number, std::generic_category());
    return std::string(operation) + ": " + error.message() +
           " (errno " + std::to_string(error_number) + ")";
}

bool socket_io::configure_for_writes(const int socket_fd) {
// Writing to a socket the peer has closed raises SIGPIPE, which kills the process
// by default. On macOS, SO_NOSIGPIPE turns that into an EPIPE error instead. Linux
// has no such option; send_flags() passes MSG_NOSIGNAL on every send instead.
#ifdef SO_NOSIGPIPE
    const int disable_sigpipe = 1;
    return setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
                      &disable_sigpipe, sizeof(disable_sigpipe)) == 0;
#else
    return true;
#endif
}

ssize_t socket_io::send_some(const int socket_fd, const std::string_view data) {
    return send(socket_fd, data.data(), data.size(), send_flags());
}

bool socket_io::send_all(const int socket_fd, const std::string_view data) {
    std::size_t bytes_sent = 0;

    while (bytes_sent < data.size()) {
        const ssize_t result = send_some(socket_fd, data.substr(bytes_sent));
        if (result > 0) {
            bytes_sent += static_cast<std::size_t>(result);
            continue;
        }

        const int error_number = result < 0 ? errno : EIO;
        if (error_number == EINTR) {
            continue;
        }

        errno = error_number;
        return false;
    }

    return true;
}
