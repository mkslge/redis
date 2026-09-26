#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Client.h"

namespace {
class SocketHandle {
public:
    explicit SocketHandle(const int descriptor) : descriptor_(descriptor) {}
    ~SocketHandle() {
        if (descriptor_ >= 0) close(descriptor_);
    }

    int get() const { return descriptor_; }

    void reset() {
        if (descriptor_ >= 0) close(descriptor_);
        descriptor_ = -1;
    }

private:
    int descriptor_;
};

std::uint16_t bind_socket_to_unused_loopback_port(const int socket_fd) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        throw std::runtime_error("Failed to bind test socket");
    }

    socklen_t address_size = sizeof(address);
    if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &address_size) != 0) {
        throw std::runtime_error("Failed to get test socket address");
    }
    return ntohs(address.sin_port);
}

template <typename Action>
std::string thrown_socket_error(Action action) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }

    ADD_FAILURE() << "Expected the socket operation to throw";
    return {};
}

void expect_errno_details(const std::string& message,
                          const std::string& operation,
                          const std::initializer_list<int> possible_errors) {
    EXPECT_NE(message.find(operation), std::string::npos) << message;

    for (const int error_number : possible_errors) {
        const std::string description = std::strerror(error_number);
        const std::string number = "errno " + std::to_string(error_number);
        if (message.find(description) != std::string::npos &&
            message.find(number) != std::string::npos) {
            return;
        }
    }

    ADD_FAILURE() << "Missing matching errno description and number in: " << message;
}
}

TEST(ClientTest, BuildServerAddressUsesRequestedPortAndIp) {
    const sockaddr_in address = Client::build_server_address("127.0.0.1", 6380);

    EXPECT_EQ(address.sin_family, AF_INET);
    EXPECT_EQ(ntohs(address.sin_port), 6380);
    EXPECT_EQ(address.sin_addr.s_addr, inet_addr("127.0.0.1"));
}

TEST(ClientTest, BuildServerAddressRejectsInvalidIpv4Address) {
    EXPECT_THROW(Client::build_server_address("not_an_ip", Client::kDefaultPort), std::invalid_argument);
}

TEST(ClientTest, ResponseFromBufferUsesExactByteCount) {
    const char raw_response[] = {'O', 'K', '\0', 'X'};

    const std::string response = Client::response_from_buffer(raw_response, 4);

    ASSERT_EQ(response.size(), 4U);
    EXPECT_EQ(response[0], 'O');
    EXPECT_EQ(response[1], 'K');
    EXPECT_EQ(response[2], '\0');
    EXPECT_EQ(response[3], 'X');
}

TEST(ClientTest, SendCommandSendsCompleteNewlineTerminatedCommand) {
    SocketHandle listener(socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_GE(listener.get(), 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    ASSERT_EQ(bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
    ASSERT_EQ(listen(listener.get(), 1), 0);

    socklen_t address_size = sizeof(address);
    ASSERT_EQ(getsockname(listener.get(), reinterpret_cast<sockaddr*>(&address), &address_size), 0);
    Client client("127.0.0.1", ntohs(address.sin_port));
    SocketHandle connection(accept(listener.get(), nullptr, nullptr));
    ASSERT_GE(connection.get(), 0);

    ASSERT_TRUE(client.send_command("SET key value"));

    std::string received;
    char buffer[32];
    while (received.find('\n') == std::string::npos) {
        const ssize_t count = recv(connection.get(), buffer, sizeof(buffer), 0);
        ASSERT_GT(count, 0);
        received.append(buffer, static_cast<std::size_t>(count));
    }
    EXPECT_EQ(received, "SET key value\n");
}

TEST(ClientIntegrationTest, ConnectFailureIncludesErrnoDetails) {
    SocketHandle unavailable_endpoint(socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_GE(unavailable_endpoint.get(), 0);
    const std::uint16_t port = bind_socket_to_unused_loopback_port(unavailable_endpoint.get());
    unavailable_endpoint.reset();

    const std::string message = thrown_socket_error([&] {
        Client client("127.0.0.1", port);
    });

    expect_errno_details(message, "connect", {ECONNREFUSED});
}

TEST(ClientIntegrationTest, SendFailureIncludesErrnoDetails) {
    SocketHandle listener(socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_GE(listener.get(), 0);
    const std::uint16_t port = bind_socket_to_unused_loopback_port(listener.get());
    ASSERT_EQ(listen(listener.get(), 1), 0);

    Client client("127.0.0.1", port);
    SocketHandle connection(accept(listener.get(), nullptr, nullptr));
    ASSERT_GE(connection.get(), 0);
    const linger reset_on_close{.l_onoff = 1, .l_linger = 0};
    ASSERT_EQ(setsockopt(connection.get(), SOL_SOCKET, SO_LINGER,
                         &reset_on_close, sizeof(reset_on_close)), 0);
    connection.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const std::string message = thrown_socket_error([&] {
        client.send_command("PING");
    });

    expect_errno_details(message, "send", {EPIPE, ECONNRESET});
}

TEST(ClientIntegrationTest, ReceiveFailureIncludesErrnoDetails) {
    SocketHandle listener(socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_GE(listener.get(), 0);
    const std::uint16_t port = bind_socket_to_unused_loopback_port(listener.get());
    ASSERT_EQ(listen(listener.get(), 1), 0);

    Client client("127.0.0.1", port);
    SocketHandle connection(accept(listener.get(), nullptr, nullptr));
    ASSERT_GE(connection.get(), 0);
    const linger reset_on_close{.l_onoff = 1, .l_linger = 0};
    ASSERT_EQ(setsockopt(connection.get(), SOL_SOCKET, SO_LINGER,
                         &reset_on_close, sizeof(reset_on_close)), 0);
    connection.reset();

    const std::string message = thrown_socket_error([&] {
        client.get_response();
    });

    expect_errno_details(message, "receive", {ECONNRESET});
}

TEST(ClientIntegrationTest, SilentServerTimesOutInsteadOfHanging) {
    SocketHandle listener(socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_GE(listener.get(), 0);
    const std::uint16_t port = bind_socket_to_unused_loopback_port(listener.get());
    ASSERT_EQ(listen(listener.get(), 1), 0);

    Client client("127.0.0.1", port, Client::kConnectTimeout, std::chrono::milliseconds(200));
    SocketHandle connection(accept(listener.get(), nullptr, nullptr));
    ASSERT_GE(connection.get(), 0);
    ASSERT_TRUE(client.send_command("GET key"));

    const auto started = std::chrono::steady_clock::now();
    const std::string message = thrown_socket_error([&] {
        client.get_response();
    });

    EXPECT_NE(message.find("timed out"), std::string::npos) << message;
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(2));
}

TEST(ClientIntegrationTest, ConnectToFullBacklogTimesOut) {
    SocketHandle listener(socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_GE(listener.get(), 0);
    const std::uint16_t port = bind_socket_to_unused_loopback_port(listener.get());
    ASSERT_EQ(listen(listener.get(), 0), 0);

    // Fill the listen queue without ever accepting, so further handshakes stall.
    std::vector<std::unique_ptr<Client>> queued;
    for (int attempt = 0; attempt < 8; ++attempt) {
        try {
            queued.push_back(std::make_unique<Client>("127.0.0.1", port, std::chrono::milliseconds(200)));
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string(error.what()).find("timed out"), std::string::npos) << error.what();
            return;
        }
    }
    GTEST_SKIP() << "the OS accepted every queued connection; cannot provoke a connect timeout here";
}
