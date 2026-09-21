#ifndef SOCKETIO_H
#define SOCKETIO_H

#include <string_view>

namespace socket_io {
    bool configure_for_writes(int socket_fd);
    bool send_all(int socket_fd, std::string_view data);
}

#endif //SOCKETIO_H
