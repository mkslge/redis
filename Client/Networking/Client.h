#ifndef CLIENT_H
#define CLIENT_H

#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

class Client {
private:
    static constexpr std::size_t kBufferSize = 1024;

    // Connects, giving up after `timeout`; throws on failure.
    void connect_with_timeout(const std::string& server_ip, std::uint16_t port,
                              std::chrono::milliseconds timeout);

    int socket_fd{-1};
    sockaddr_in server{};
    char buffer[kBufferSize]{};
    std::string pending_response_;

public:
    static constexpr std::uint16_t kDefaultPort = 6380;
    // Connecting fails after this long, e.g. against an unreachable host.
    static constexpr std::chrono::milliseconds kConnectTimeout{5'000};
    // Sending a command or waiting for its response fails after this long without
    // progress, so a stalled server can't hang the client forever.
    static constexpr std::chrono::milliseconds kResponseTimeout{30'000};

    explicit Client(const std::string& server_ip,
                    std::uint16_t port = kDefaultPort,
                    std::chrono::milliseconds connect_timeout = kConnectTimeout,
                    std::chrono::milliseconds response_timeout = kResponseTimeout);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    static sockaddr_in build_server_address(const std::string& server_ip, std::uint16_t port);
    static std::string response_from_buffer(const char* response_buffer, std::size_t bytes_read);

    bool send_command(const std::string& command);
    std::string get_response();
};


#endif //CLIENT_H
