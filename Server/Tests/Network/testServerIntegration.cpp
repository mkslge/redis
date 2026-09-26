#include "App/CommandProcessor.h"
#include "Persistence/AofWriter.h"
#include "Persistence/AofReplayer.h"
#include "Commands/Executor.h"
#include "Storage/StorageEngine.h"
#include "Network/Server.h"
#include "Protocol/RespCommandCodec.h"
#include "Networking/SocketIO.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <thread>

namespace {

std::filesystem::path make_temp_log_path(const std::string& test_name) {
    return std::filesystem::temp_directory_path() / ("redisimpl-" + test_name + "-aof.txt");
}

class TempLogFile {
public:
    explicit TempLogFile(const std::string& test_name) : path_(make_temp_log_path(test_name)) {
        std::filesystem::remove(path_);
    }

    ~TempLogFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    std::string path_string() const {
        return path_.string();
    }

    std::string read_all() const {
        std::ifstream stream(path_);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

private:
    std::filesystem::path path_;
};

class TestConnection {
public:
    explicit TestConnection(const std::uint16_t port) {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            throw std::runtime_error("Failed to create client socket");
        }

        const timeval timeout{.tv_sec = 2, .tv_usec = 0};
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
            setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
            close(socket_fd_);
            throw std::runtime_error("Failed to configure client socket timeout");
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
            close(socket_fd_);
            throw std::runtime_error("Failed to parse loopback address");
        }

        if (connect(socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            close(socket_fd_);
            throw std::runtime_error("Failed to connect to server");
        }
    }

    ~TestConnection() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }

    std::string send_command(const std::string& command) {
        std::string framed_command = command;
        if (framed_command.empty() || framed_command.back() != '\n') {
            framed_command.push_back('\n');
        }

        send_bytes(framed_command);

        return read_response();
    }

    void send_bytes(const std::string& bytes) {
        if (!socket_io::send_all(socket_fd_, bytes)) {
            throw std::runtime_error("Failed to send bytes to server");
        }
    }

    std::string read_response() {
        char buffer[1024]{};

        while (pending_response_.find('\n') == std::string::npos) {
            const ssize_t bytes_read = recv(socket_fd_, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                throw std::runtime_error("Server closed connection before response");
            }
            pending_response_.append(buffer, static_cast<std::size_t>(bytes_read));
        }

        const std::size_t newline = pending_response_.find('\n');
        std::string response = pending_response_.substr(0, newline);
        pending_response_.erase(0, newline + 1);
        return response;
    }

    bool read_until_closed() const {
        char buffer[4096];
        while (true) {
            const ssize_t bytes_read = recv(socket_fd_, buffer, sizeof(buffer), 0);
            if (bytes_read > 0) continue;
            if (bytes_read == 0) return true;
            if (errno == EINTR) continue;
            return errno == ECONNRESET;
        }
    }

    void send_command_and_reset(const std::string& command) {
        std::string framed_command = command;
        if (framed_command.empty() || framed_command.back() != '\n') {
            framed_command.push_back('\n');
        }

        const linger reset_on_close{.l_onoff = 1, .l_linger = 0};
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_LINGER, &reset_on_close, sizeof(reset_on_close)) != 0) {
            throw std::runtime_error("Failed to configure reset-on-close");
        }
        if (!socket_io::send_all(socket_fd_, framed_command)) {
            throw std::runtime_error("Failed to send command before reset");
        }

        // Reset only once the response has started arriving, so the server is
        // mid-send rather than still reading or formatting.
        char first_byte;
        if (recv(socket_fd_, &first_byte, 1, 0) != 1) {
            throw std::runtime_error("No response arrived before reset");
        }
        close(socket_fd_);
        socket_fd_ = -1;
    }

    void reset_connection() {
        const linger reset_on_close{.l_onoff = 1, .l_linger = 0};
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_LINGER, &reset_on_close, sizeof(reset_on_close)) != 0) {
            throw std::runtime_error("Failed to configure reset-on-close");
        }
        close(socket_fd_);
        socket_fd_ = -1;
    }

private:
    int socket_fd_{-1};
    std::string pending_response_;
};

class ServerHarness {
public:
    explicit ServerHarness(
        const std::string& log_path,
        const std::chrono::milliseconds expiration_sweep_interval = std::chrono::milliseconds(100),
        const ClientSession::Timeouts client_timeouts = Server::kDefaultClientTimeouts)
        : storage_(),
          executor_(storage_),
          command_processor_(executor_),
          aof_writer_(log_path, AofFsyncPolicy::EVERY_SECOND),
          server_(aof_writer_, command_processor_, storage_, 0, expiration_sweep_interval,
                  client_timeouts),
          thread_([this] { server_.run(); }) {}

    ~ServerHarness() {
        shutdown();
    }

    ServerHarness(const ServerHarness&) = delete;
    ServerHarness& operator=(const ServerHarness&) = delete;

    std::uint16_t port() const {
        return server_.port();
    }

    StorageEngine& storage() {
        return storage_;
    }

    void request_stop() {
        server_.stop();
    }

    void join() {
        if (thread_.joinable()) thread_.join();
        stopped_ = true;
    }

    void shutdown() {
        if (!stopped_) {
            request_stop();
            join();
        }
    }

private:
    StorageEngine storage_;
    Executor executor_;
    CommandProcessor command_processor_;
    AofWriter aof_writer_;
    Server server_;
    std::thread thread_;
    bool stopped_{false};
};

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

} // namespace



TEST(ServerIntegrationTest, RestartReplaysAppendOnlyLogAndRestoresState) {
    TempLogFile log_file("server-restart-replay");

    {
        ServerHarness first_server(log_file.path_string());
        TestConnection connection(first_server.port());

        EXPECT_EQ(connection.send_command("SET \"user\" \"alice\""), "SET value=\"alice\"");
        EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", "user", "alice"}));
        EXPECT_EQ(connection.send_command("EXPIRE \"user\" 30"), "EXPIRE applied=true");
        EXPECT_EQ(connection.send_command("QUIT"), "BYE");
        first_server.shutdown();
    }

    StorageEngine restarted_storage;
    Executor restarted_executor(restarted_storage);
    AofReplayer aof_replayer(log_file.path_string());

    aof_replayer.replay(restarted_executor);

    ASSERT_TRUE(restarted_storage.get("user").has_value());
    EXPECT_EQ(restarted_storage.get("user")->bytes(), "alice");
    EXPECT_TRUE(restarted_storage.exists("user"));

}

TEST(ServerIntegrationTest, BinaryKeysAndValuesRoundTripThroughEscapesAndRestart) {
    TempLogFile log_file("binary-end-to-end");
    const Bytes key{"k\0\n", 3};
    const Bytes value{"\0\r\n\"\\\xff end", 10};
    const std::string escaped_key = R"("k\x00\n")";
    const std::string escaped_value = R"("\x00\r\n\"\\\xff end")";

    {
        ServerHarness first_server(log_file.path_string());
        TestConnection connection(first_server.port());
        EXPECT_EQ(connection.send_command("SET " + escaped_key + " " + escaped_value),
                  "SET value=" + escaped_value);
        EXPECT_EQ(connection.send_command("GET " + escaped_key), "GET value=" + escaped_value);
        ASSERT_TRUE(first_server.storage().get(key).has_value());
        EXPECT_EQ(first_server.storage().get(key)->bytes(), value);
        first_server.shutdown();
    }
    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", key, value}));

    ServerHarness restarted_server(log_file.path_string());
    Executor replay_executor(restarted_server.storage());
    AofReplayer(log_file.path_string()).replay(replay_executor);
    TestConnection connection(restarted_server.port());
    EXPECT_EQ(connection.send_command("GET " + escaped_key), "GET value=" + escaped_value);
}

TEST(ServerIntegrationTest, FailedResponseSendCleansUpClientSession) {
    TempLogFile log_file("failed-response-cleanup");
    ServerHarness server(log_file.path_string());
    server.storage().set("large", Value(std::string(16 * 1024 * 1024, 'x')));
    testing::internal::CaptureStderr();

    {
        TestConnection disconnected_client(server.port());
        disconnected_client.send_command_and_reset("GET \"large\"");
    }

    bool reconnected = false;
    for (int attempt = 0; attempt < 50 && !reconnected; ++attempt) {
        try {
            TestConnection connection(server.port());
            reconnected = connection.send_command("EXISTS \"large\"") == "EXISTS exists=true";
        } catch (const std::exception&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    EXPECT_TRUE(reconnected);
    server.shutdown();
    const std::string error_output = testing::internal::GetCapturedStderr();
    expect_errno_details(error_output, "send", {EPIPE, ECONNRESET});
}

TEST(ServerIntegrationTest, FailedReceiveIncludesErrnoDetails) {
    TempLogFile log_file("failed-receive-details");
    ServerHarness server(log_file.path_string());
    testing::internal::CaptureStderr();

    {
        TestConnection connection(server.port());
        connection.send_bytes("partial command");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        connection.reset_connection();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.shutdown();

    const std::string error_output = testing::internal::GetCapturedStderr();
    expect_errno_details(error_output, "receive", {ECONNRESET});
}

TEST(ServerIntegrationTest, ShutdownWakesEventLoopWithIdleClient) {
    TempLogFile log_file("idle-client-shutdown");
    ServerHarness server(log_file.path_string());
    TestConnection idle_connection(server.port());

    server.shutdown();
}

TEST(ServerIntegrationTest, RapidReconnectDoesNotReuseStaleSessionState) {
    TempLogFile log_file("descriptor-reuse");
    ServerHarness server(log_file.path_string());

    for (int attempt = 0; attempt < 50; ++attempt) {
        {
            TestConnection incomplete(server.port());
            incomplete.send_bytes("SET \"abandoned\"");
        }

        TestConnection next(server.port());
        EXPECT_EQ(next.send_command("EXISTS \"abandoned\""), "EXISTS exists=false");
    }
}

TEST(ServerIntegrationTest, ShutdownClosesClientsInDifferentSessionStates) {
    TempLogFile log_file("mixed-session-shutdown");
    ServerHarness server(log_file.path_string());
    TestConnection idle(server.port());
    TestConnection partial(server.port());
    TestConnection unread_response(server.port());

    partial.send_bytes("SET \"partial\"");
    unread_response.send_bytes("EXISTS \"missing\"\n");

    server.shutdown();

    EXPECT_TRUE(idle.read_until_closed());
    EXPECT_TRUE(partial.read_until_closed());
    EXPECT_TRUE(unread_response.read_until_closed());
}

TEST(ServerIntegrationTest, UnfinishedRequestLineTimesOutButIdleClientStays) {
    TempLogFile log_file("partial-line-timeout");
    const ClientSession::Timeouts short_timeouts{
        std::chrono::milliseconds(200), std::chrono::seconds(30)};
    ServerHarness server(log_file.path_string(), std::chrono::milliseconds(50), short_timeouts);
    TestConnection idle(server.port());
    TestConnection trickling(server.port());

    trickling.send_bytes("SET \"k\"");
    EXPECT_TRUE(trickling.read_until_closed());

    // Well past the line timeout, a client that sent nothing is still connected.
    EXPECT_EQ(idle.send_command("EXISTS k"), "EXISTS exists=false");
}

TEST(ServerIntegrationTest, ClientThatStopsReadingTimesOut) {
    TempLogFile log_file("write-stall-timeout");
    const ClientSession::Timeouts short_timeouts{
        std::chrono::seconds(30), std::chrono::milliseconds(200)};
    ServerHarness server(log_file.path_string(), std::chrono::milliseconds(50), short_timeouts);
    // Far larger than the socket buffers, so the response can't be fully sent.
    server.storage().set("large", Value(std::string(16 * 1024 * 1024, 'x')));
    testing::internal::CaptureStderr();

    TestConnection stalled(server.port());
    stalled.send_bytes("GET large\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    server.shutdown();
    EXPECT_NE(testing::internal::GetCapturedStderr().find("no response bytes accepted in time"),
              std::string::npos);
}

TEST(ServerIntegrationTest, StoppedServerRefusesNewConnections) {
    TempLogFile log_file("refuse-after-stop");
    ServerHarness server(log_file.path_string());
    const std::uint16_t port = server.port();
    server.shutdown();

    // The listening socket is closed when run() returns, so a late client is refused
    // immediately instead of waiting in the listen queue until the Server is destroyed.
    EXPECT_THROW(TestConnection late_client(port), std::runtime_error);
}

TEST(ServerIntegrationTest, ConcurrentStopRequestsAreIdempotent) {
    TempLogFile log_file("concurrent-stop");
    ServerHarness server(log_file.path_string());
    TestConnection idle(server.port());
    std::vector<std::thread> stoppers;

    for (int index = 0; index < 8; ++index) {
        stoppers.emplace_back([&server] { server.request_stop(); });
    }
    for (auto& stopper : stoppers) stopper.join();
    server.join();

    EXPECT_TRUE(idle.read_until_closed());
}

TEST(ServerIntegrationTest, PartialCommandFromOneClientDoesNotBlockAnother) {
    TempLogFile log_file("partial-command-fairness");
    ServerHarness server(log_file.path_string());
    TestConnection partial_client(server.port());
    TestConnection complete_client(server.port());

    partial_client.send_bytes("SET \"partial\"");

    EXPECT_EQ(complete_client.send_command("SET \"complete\" \"value\""),
              "SET value=\"value\"");

    partial_client.send_bytes(" \"finished\"\n");
    EXPECT_EQ(partial_client.read_response(), "SET value=\"finished\"");
}

TEST(ServerIntegrationTest, PipelinedCommandsPreserveResponseOrder) {
    TempLogFile log_file("pipelined-commands");
    ServerHarness server(log_file.path_string());
    TestConnection connection(server.port());

    connection.send_bytes("SET \"key\" \"value\"\nGET \"key\"\n");

    EXPECT_EQ(connection.read_response(), "SET value=\"value\"");
    EXPECT_EQ(connection.read_response(), "GET value=\"value\"");
}

TEST(ServerIntegrationTest, SlowReaderDoesNotBlockOtherClients) {
    TempLogFile log_file("slow-reader-fairness");
    ServerHarness server(log_file.path_string());
    server.storage().set("large", Value(std::string(16 * 1024 * 1024, 'x')));
    TestConnection slow_reader(server.port());
    TestConnection active_client(server.port());

    slow_reader.send_bytes("GET \"large\"\n");

    EXPECT_EQ(active_client.send_command("EXISTS \"large\""), "EXISTS exists=true");
}

TEST(ServerIntegrationTest, EventLoopPrunesExpiredKeysOnTimer) {
    TempLogFile log_file("event-loop-expiration");
    ServerHarness server(log_file.path_string(), std::chrono::milliseconds(5));
    server.storage().set("short-lived", Value("value"));
    ASSERT_TRUE(server.storage().expire("short-lived", std::chrono::milliseconds(5)));

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_FALSE(server.storage().keys_with_deadlines().contains("short-lived"));
}

TEST(ServerIntegrationTest, NumericCommandsReturnResultsAndPersistBytes) {
    TempLogFile log_file("numeric-command-responses");
    ServerHarness server(log_file.path_string());
    TestConnection connection(server.port());

    EXPECT_EQ(connection.send_command("INCR \"counter\""), "INCR value=1");
    EXPECT_EQ(connection.send_command("INCRBY \"counter\" 9"), "INCRBY value=10");
    EXPECT_EQ(connection.send_command("DECR \"counter\""), "DECR value=9");
    EXPECT_EQ(connection.send_command("DECRBY \"counter\" 4"), "DECRBY value=5");
    EXPECT_EQ(connection.send_command("GET \"counter\""), "GET value=\"5\"");
}

TEST(ServerIntegrationTest, NumericRuntimeErrorDoesNotModifyValueOrCloseConnection) {
    TempLogFile log_file("numeric-command-error");
    ServerHarness server(log_file.path_string());
    TestConnection connection(server.port());

    EXPECT_EQ(connection.send_command("SET \"counter\" \"not-an-integer\""),
              "SET value=\"not-an-integer\"");
    EXPECT_EQ(connection.send_command("INCR \"counter\""),
              "ERROR value is not an integer or out of range");
    EXPECT_EQ(connection.send_command("GET \"counter\""),
              "GET value=\"not-an-integer\"");
}

TEST(ServerIntegrationTest, ValueWithNewlineStaysOnOneResponseLine) {
    TempLogFile log_file("newline-value-response");
    ServerHarness server(log_file.path_string());
    TestConnection connection(server.port());

    // An unescaped newline in a value used to split one response into two lines,
    // leaving every later response off by one.
    EXPECT_EQ(connection.send_command(R"(SET k "a\nb")"), R"(SET value="a\nb")");
    EXPECT_EQ(connection.send_command("EXISTS k"), "EXISTS exists=true");
    EXPECT_EQ(connection.send_command("GET k"), R"(GET value="a\nb")");
}

TEST(ServerIntegrationTest, BindFailureIncludesErrnoDetails) {
    TempLogFile log_file("bind-error-details");
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor command_processor(executor);
    AofWriter aof_writer(log_file.path_string(), AofFsyncPolicy::EVERY_SECOND);
    Server first_server(aof_writer, command_processor, storage, 0);

    std::string message;
    try {
        Server conflicting_server(aof_writer, command_processor, storage, first_server.port());
    } catch (const std::exception& error) {
        message = error.what();
    }

    ASSERT_FALSE(message.empty()) << "Expected the second server to fail to bind";
    expect_errno_details(message, "bind", {EADDRINUSE});
}
