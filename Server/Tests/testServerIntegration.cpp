#include "CommandProcessor.h"
#include "AOFLogger.h"
#include "LogRunner.h"
#include "Executor.h"
#include "StorageEngine.h"
#include "Server.h"
#include "RespCommandCodec.h"
#include "SocketIO.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
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

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
        const std::chrono::milliseconds expiration_sweep_interval = std::chrono::milliseconds(100))
        : storage_(),
          executor_(storage_),
          command_processor_(executor_),
          logger_(log_path, AOFFsyncPolicy::EVERY_SECOND),
          server_(logger_, command_processor_, storage_, 0, expiration_sweep_interval),
          thread_([this] { server_.run(); }) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

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

    void shutdown() {
        if (!stopped_) {
            server_.stop();
            if (thread_.joinable()) {
                thread_.join();
            }
            stopped_ = true;
        }
    }

private:
    StorageEngine storage_;
    Executor executor_;
    CommandProcessor command_processor_;
    AOFLogger logger_;
    Server server_;
    std::thread thread_;
    bool stopped_{false};
};

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
    CommandProcessor restarted_processor(restarted_executor);
    LogRunner log_runner(log_file.path_string());

    log_runner.run_log(restarted_processor);

    ASSERT_TRUE(restarted_storage.get("user").has_value());
    EXPECT_EQ(restarted_storage.get("user")->bytes(), "alice");
    EXPECT_TRUE(restarted_storage.exists("user"));

}

TEST(ServerIntegrationTest, FailedResponseSendCleansUpClientSession) {
    TempLogFile log_file("failed-response-cleanup");
    ServerHarness server(log_file.path_string());
    server.storage().set("large", Value(std::string(16 * 1024 * 1024, 'x')));

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
}

TEST(ServerIntegrationTest, ShutdownWakesEventLoopWithIdleClient) {
    TempLogFile log_file("idle-client-shutdown");
    ServerHarness server(log_file.path_string());
    TestConnection idle_connection(server.port());

    server.shutdown();
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

    EXPECT_FALSE(server.storage().possibly_expired().contains("short-lived"));
}
