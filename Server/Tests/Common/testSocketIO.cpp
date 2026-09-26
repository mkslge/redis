#include "Networking/SocketIO.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
class SocketPair {
public:
    SocketPair() {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_) != 0) {
            throw std::runtime_error("Failed to create socket pair");
        }
    }

    ~SocketPair() {
        for (const int socket : sockets_) {
            if (socket >= 0) close(socket);
        }
    }

    int sender() const { return sockets_[0]; }
    int receiver() const { return sockets_[1]; }

    void close_receiver() {
        close(sockets_[1]);
        sockets_[1] = -1;
    }

private:
    int sockets_[2]{-1, -1};
};
}

TEST(SocketIOTest, EmptyPayloadSucceeds) {
    SocketPair sockets;

    EXPECT_TRUE(socket_io::send_all(sockets.sender(), ""));
}

TEST(SocketIOTest, SendsLargePayloadExactly) {
    SocketPair sockets;
    const int small_buffer = 1024;
    ASSERT_EQ(setsockopt(sockets.sender(), SOL_SOCKET, SO_SNDBUF,
                         &small_buffer, sizeof(small_buffer)), 0);
    const std::string payload(1024 * 1024, 'x');
    std::string received;

    std::thread reader([&] {
        char buffer[4096];
        while (received.size() < payload.size()) {
            const ssize_t count = recv(sockets.receiver(), buffer, sizeof(buffer), 0);
            if (count <= 0) return;
            received.append(buffer, static_cast<std::size_t>(count));
        }
    });

    const bool sent = socket_io::send_all(sockets.sender(), payload);
    shutdown(sockets.sender(), SHUT_WR);
    reader.join();

    EXPECT_TRUE(sent);
    EXPECT_EQ(received, payload);
}

TEST(SocketIOTest, ClosedPeerReturnsFalseWithoutRaisingSigpipe) {
    SocketPair sockets;
    ASSERT_TRUE(socket_io::configure_for_writes(sockets.sender()));
    sockets.close_receiver();

    EXPECT_FALSE(socket_io::send_all(sockets.sender(), "message"));
}
