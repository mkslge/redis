#ifndef SERVER_H
#define SERVER_H

#include "Log/AOFLogger.h"
#include "Interpreter/Runtime/CommandHandler.h"

#include <arpa/inet.h>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

class Server {
private:
    static constexpr std::size_t kBufferSize = 1024;

    std::uint16_t port_;
    int socket_fd_{-1};
    sockaddr_in serveraddr_{};
    AOFLogger& logger_;
    CommandHandler& command_handler_;

    void bind_and_listen();
    static bool send_response(int client_fd, const std::string& response);
    void handle_client(int client_fd) const;

public:
    static constexpr std::uint16_t kDefaultPort = 6380;

    explicit Server(AOFLogger& logger, CommandHandler& command_handler, std::uint16_t port = kDefaultPort);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
};

#endif //SERVER_H
