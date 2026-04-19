#ifndef SERVER_H
#define SERVER_H

#include "App/CommandProcessor.h"
#include "Persistence/AOFLogger.h"

#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <unordered_map>
#include <mutex>

class Server {
private:
    static constexpr std::size_t kBufferSize = 1024;

    struct ClientSession {
        std::thread worker;
        bool finished{false};
    };

    std::uint16_t port_;
    int socket_fd_{-1};
    sockaddr_in serveraddr_{};
    AOFLogger& logger_;
    CommandProcessor& command_processor_;
    std::atomic<bool> stopping_{false};
    std::unordered_map<int, ClientSession> clients_;
    std::mutex mutex_;

    void bind_and_listen();
    static bool send_response(int client_fd, const std::string& response);
    void handle_client(int client_fd);
    void mark_client_finished(int client_fd);
    void reap_finished_clients();
    void shutdown_clients();

public:
    static constexpr std::uint16_t kDefaultPort = 6380;

    explicit Server(AOFLogger& logger, CommandProcessor& command_processor, std::uint16_t port = kDefaultPort);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop();
    std::uint16_t port() const;
};

#endif //SERVER_H
