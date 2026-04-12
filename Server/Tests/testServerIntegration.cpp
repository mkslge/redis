#include "App/CommandProcessor.h"
#include "Persistence/AOFLogger.h"
#include "Persistence/LogRunner.h"
#include "Runtime/Executor.h"
#include "Runtime/StorageEngine.h"
#include "TCP/Server.h"

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

        const ssize_t sent = send(socket_fd_, framed_command.c_str(), framed_command.size(), 0);
        if (sent <= 0) {
            throw std::runtime_error("Failed to send command to server");
        }

        return read_response();
    }

private:
    std::string read_response() {
        std::string response;
        char buffer[1024]{};

        while (response.find('\n') == std::string::npos) {
            const ssize_t bytes_read = recv(socket_fd_, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                throw std::runtime_error("Server closed connection before response");
            }
            response.append(buffer, static_cast<std::size_t>(bytes_read));
        }

        response.resize(response.find('\n'));
        return response;
    }

    int socket_fd_{-1};
};

class ServerHarness {
public:
    explicit ServerHarness(const std::string& log_path)
        : storage_(),
          executor_(storage_),
          command_processor_(executor_),
          logger_(log_path),
          server_(logger_, command_processor_, 0),
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

TEST(ServerIntegrationTest, StartupAndCommandHandlingPersistMutationsOverTcp) {
    TempLogFile log_file("server-startup-command-handling");
    ServerHarness server(log_file.path_string());
    TestConnection connection(server.port());

    EXPECT_EQ(connection.send_command("SET \"language\" \"C++\""), "SET value=\"C++\"");
    EXPECT_EQ(connection.send_command("GET \"language\""), "GET value=\"C++\"");
    EXPECT_EQ(connection.send_command("QUIT"), "BYE");

    EXPECT_TRUE(server.storage().get("language").has_value());
    EXPECT_EQ(log_file.read_all(), "SET \"language\" \"C++\"\n");
}

TEST(ServerIntegrationTest, RestartReplaysAppendOnlyLogAndRestoresState) {
    TempLogFile log_file("server-restart-replay");

    {
        ServerHarness first_server(log_file.path_string());
        TestConnection connection(first_server.port());

        EXPECT_EQ(connection.send_command("SET \"user\" \"alice\""), "SET value=\"alice\"");
        EXPECT_EQ(connection.send_command("EXPIRE \"user\" 30"), "EXPIRE applied=true");
        EXPECT_EQ(connection.send_command("QUIT"), "BYE");
    }

    StorageEngine restarted_storage;
    Executor restarted_executor(restarted_storage);
    CommandProcessor restarted_processor(restarted_executor);
    LogRunner log_runner(log_file.path_string());

    log_runner.run_log(restarted_processor);

    ASSERT_TRUE(restarted_storage.get("user").has_value());
    EXPECT_EQ(restarted_storage.get("user")->get<std::string>(), "alice");
    EXPECT_TRUE(restarted_storage.exists("user"));
}

TEST(ServerIntegrationTest, InvalidCommandsReturnErrorsAndDoNotWriteToAppendOnlyLog) {
    TempLogFile log_file("server-invalid-command");
    ServerHarness server(log_file.path_string());
    TestConnection connection(server.port());

    EXPECT_EQ(connection.send_command("SET \"user\""), "ERROR parse failure");
    EXPECT_EQ(connection.send_command("bogus"), "ERROR invalid command");
    EXPECT_EQ(connection.send_command("QUIT"), "BYE");

    EXPECT_TRUE(log_file.read_all().empty());
    EXPECT_FALSE(server.storage().exists("user"));
}
