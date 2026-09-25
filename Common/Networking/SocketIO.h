#ifndef SOCKETIO_H
#define SOCKETIO_H

#include <string>
#include <string_view>
#include <sys/types.h>

namespace socket_io {
    std::string error_message(std::string_view operation, int error_number);
    bool configure_for_writes(int socket_fd);
    ssize_t send_some(int socket_fd, std::string_view data);
    bool send_all(int socket_fd, std::string_view data);
}

#endif //SOCKETIO_H
