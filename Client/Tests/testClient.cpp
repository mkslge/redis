#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <string>

#include "Networking/Client.h"

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
