#ifndef SERVER_H
#define SERVER_H

#include "AOFLogger.h"
#include "CommandProcessor.h"
#include "StorageEngine.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

class Server {
private:
    static constexpr std::size_t kBufferSize = 1024;
    static constexpr std::size_t kMaxReadPerEvent = 64 * 1024;
    static constexpr std::size_t kMaxWritePerEvent = 64 * 1024;
    static constexpr std::size_t kMaxInputBuffer = 1024 * 1024;
    static constexpr std::size_t kMaxOutputBuffer = 32 * 1024 * 1024;
    static constexpr std::size_t kExpirationCandidatesPerSweep = 200;

    struct ClientSession {
        std::string input_buffer;
        std::string output_buffer;
        std::size_t output_offset{0};
        bool close_after_write{false};
    };

    std::uint16_t port_;
    int socket_fd_{-1};
    int wakeup_fds_[2]{-1, -1};
    sockaddr_in serveraddr_{};
    AOFLogger& logger_;
    CommandProcessor& command_processor_;
    StorageEngine& storage_;
    std::chrono::milliseconds expiration_sweep_interval_;
    std::atomic<bool> stopping_{false};
    std::unordered_map<int, ClientSession> clients_;

    void bind_and_listen();
    void initialize_wakeup_pipe();
    void accept_ready_clients();
    bool read_from_client(int client_fd);
    bool process_client_input(int client_fd);
    bool flush_client_output(int client_fd);
    bool queue_response(ClientSession& session, std::string response);
    CommandProcessResult process_and_persist(const std::string& command);
    void close_client(int client_fd);
    void close_all_clients();
    void drain_wakeup_pipe() const;
    void run_expiration_sweep();

public:
    static constexpr std::uint16_t kDefaultPort = 6380;

    explicit Server(AOFLogger& logger,
                    CommandProcessor& command_processor,
                    StorageEngine& storage,
                    std::uint16_t port = kDefaultPort,
                    std::chrono::milliseconds expiration_sweep_interval = std::chrono::milliseconds(100));
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop();
    std::uint16_t port() const;
};

#endif //SERVER_H
