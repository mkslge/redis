#include "SocketIO.h"

#include <cerrno>
#include <sys/socket.h>

namespace {
int send_flags() {
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}
}

bool socket_io::configure_for_writes(const int socket_fd) {
#ifdef SO_NOSIGPIPE
    const int disable_sigpipe = 1;
    return setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
                      &disable_sigpipe, sizeof(disable_sigpipe)) == 0;
#else
    return true;
#endif
}

bool socket_io::send_all(const int socket_fd, const std::string_view data) {
    std::size_t bytes_sent = 0;

    while (bytes_sent < data.size()) {
        const ssize_t result = send(socket_fd,
                                    data.data() + bytes_sent,
                                    data.size() - bytes_sent,
                                    send_flags());
        if (result > 0) {
            bytes_sent += static_cast<std::size_t>(result);
            continue;
        }

        if (result < 0 && errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}
