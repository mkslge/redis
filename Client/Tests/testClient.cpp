#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "Client.h"

namespace {
class SocketHandle {
public:
    explicit SocketHandle(const int descriptor) : descriptor_(descriptor) {}
    ~SocketHandle() {
        if (descriptor_ >= 0) close(descriptor_);
    }

    int get() const { return descriptor_; }

private:
    int descriptor_;
};
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
