#ifndef CLIENT_H
#define CLIENT_H

#include <cstring>
#include <arpa/inet.h>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

class Client {
private:
    static constexpr std::size_t kBufferSize = 1024;

    int socket_fd{-1};
    sockaddr_in server{};
    char buffer[kBufferSize]{};
    std::string pending_response_;

public:
    static constexpr std::uint16_t kDefaultPort = 6380;

    explicit Client(const std::string& server_ip, std::uint16_t port = kDefaultPort);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    static sockaddr_in build_server_address(const std::string& server_ip, std::uint16_t port);
    static std::string response_from_buffer(const char* response_buffer, std::size_t bytes_read);

    void send_command(const std::string& command);
    std::string get_response();
};


#endif //CLIENT_H
