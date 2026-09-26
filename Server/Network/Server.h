#ifndef SERVER_H
#define SERVER_H

#include "App/CommandProcessor.h"
#include "Network/ClientSession.h"
#include "Persistence/AofWriter.h"
#include "Storage/StorageEngine.h"

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
    static constexpr std::size_t kExpirationCandidatesPerSweep = 200;

    std::uint16_t port_;
    int socket_fd_{-1};
    int wakeup_fds_[2]{-1, -1};
    sockaddr_in serveraddr_{};
    AofWriter& aof_writer_;
    CommandProcessor& command_processor_;
    StorageEngine& storage_;
    std::chrono::milliseconds expiration_sweep_interval_;
    ClientSession::Timeouts client_timeouts_;
    std::atomic<bool> stopping_{false};
    std::unordered_map<int, ClientSession> clients_;

    // Binds the listening socket and records its kernel-assigned port when port zero is requested.
    void bind_and_listen();
    // Creates the nonblocking pipe used to interrupt poll() from Server::stop().
    void initialize_wakeup_pipe();
    // Accepts every connection currently queued on the nonblocking listening socket.
    void accept_ready_clients();
    // Reads one fair-share batch into a session and reports whether the connection should remain open.
    bool read_from_client(int client_fd, ClientSession& session);
    // Processes complete commands already buffered for one session.
    bool process_client_input(ClientSession& session);
    // Writes one fair-share batch from a session and reports whether the connection should remain open.
    bool flush_client_output(int client_fd, ClientSession& session);
    // Executes one command and persists successful mutations before their response is sent.
    CommandProcessResult process_and_persist(const std::string& command);
    // Closes and erases one session; only the run thread may call this method.
    void close_client(int client_fd);
    // Closes every remaining session; only the run thread or a non-running destructor may call this method.
    void close_all_clients();
    // Consumes pending wakeup bytes so the pipe remains usable for later stop requests.
    void drain_wakeup_pipe() const;
    // Removes a bounded number of expired keys without monopolizing the event loop.
    void run_expiration_sweep();
    // Closes clients stuck mid-request or not reading their responses.
    void close_timed_out_clients();

public:
    static constexpr std::uint16_t kDefaultPort = 6380;
    // A client has 10 s to finish a request line, and pending output may go 30 s
    // without any bytes being sent. Idle clients are never closed.
    static constexpr ClientSession::Timeouts kDefaultClientTimeouts{
        std::chrono::seconds(10), std::chrono::seconds(30)};

    // Creates a listening server but does not start its blocking event loop.
    explicit Server(AofWriter& aof_writer,
                    CommandProcessor& command_processor,
                    StorageEngine& storage,
                    std::uint16_t port = kDefaultPort,
                    std::chrono::milliseconds expiration_sweep_interval = std::chrono::milliseconds(100),
                    ClientSession::Timeouts client_timeouts = kDefaultClientTimeouts);
    // Releases descriptors after run() has returned; destroying a running server is invalid.
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Runs the single owner of all client sessions; call at most once and from only one thread.
    void run();
    // Requests shutdown from any thread; the run thread performs all client cleanup.
    void stop();
    // Returns the bound port, which is immutable after construction.
    std::uint16_t port() const;
};

#endif //SERVER_H
