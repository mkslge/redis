#include "LogCompactor.h"
#include "RespCommandCodec.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path make_temp_log_path(const std::string& test_name) {
    const auto temp_dir = std::filesystem::temp_directory_path();
    return temp_dir / ("redisimpl-logcompactor-" + test_name + ".txt");
}

class TempLogFile {
public:
    TempLogFile(const std::string& test_name, const std::string& contents)
        : path_(make_temp_log_path(test_name)) {
        std::ofstream stream(path_);
        stream << contents;
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

void compact_log(const std::string& path) {
    LogCompactor compactor(path);
    compactor.compact();
}

} // namespace

TEST(LogCompactorTest, RemovesOlderSetForSameKey) {
    TempLogFile log_file(
        "older-set",
        RespCommandCodec::encode({"SET", "user", "alice"}) +
        RespCommandCodec::encode({"SET", "counter", "1"}) +
        RespCommandCodec::encode({"SET", "user", "bob"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"SET", "counter", "1"}) +
        RespCommandCodec::encode({"SET", "user", "bob"}));
}

TEST(LogCompactorTest, KeepsLatestExpireForSameKey) {
    TempLogFile log_file(
        "latest-expire",
        RespCommandCodec::encode({"SET", "session", "token"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444800000"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444860000"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"SET", "session", "token"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444860000"}));
}

TEST(LogCompactorTest, DeleteRemovesPriorMutationsForKey) {
    TempLogFile log_file(
        "delete-key",
        RespCommandCodec::encode({"SET", "session", "token"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444800000"}) +
        RespCommandCodec::encode({"DEL", "session"}) +
        RespCommandCodec::encode({"SET", "other", "value"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"DEL", "session"}) +
        RespCommandCodec::encode({"SET", "other", "value"}));
}

TEST(LogCompactorTest, LeavesIndependentKeysInOriginalOrder) {
    TempLogFile log_file(
        "independent-keys",
        RespCommandCodec::encode({"SET", "a", "1"}) +
        RespCommandCodec::encode({"SET", "b", "2"}) +
        RespCommandCodec::encode({"PEXPIREAT", "a", "4102444800000"}) +
        RespCommandCodec::encode({"SET", "c", "3"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"SET", "a", "1"}) +
        RespCommandCodec::encode({"SET", "b", "2"}) +
        RespCommandCodec::encode({"PEXPIREAT", "a", "4102444800000"}) +
        RespCommandCodec::encode({"SET", "c", "3"}));
}

TEST(LogCompactorTest, PreservesBinaryKeyAndValue) {
    const Bytes key{"k\0\n", 3};
    const Bytes old_value{"old\0", 4};
    const Bytes new_value{"new\xff", 4};
    TempLogFile log_file("binary", RespCommandCodec::encode({"SET", key, old_value}) +
                                   RespCommandCodec::encode({"SET", key, new_value}));
    LogCompactor(log_file.path_string()).compact();
    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", key, new_value}));
}

TEST(LogCompactorTest, LaterSetRemovesPriorExpirationAndDelete) {
    TempLogFile log_file("set-resets-state",
        RespCommandCodec::encode({"SET", "key", "old"}) +
        RespCommandCodec::encode({"PEXPIREAT", "key", "4102444800000"}) +
        RespCommandCodec::encode({"DEL", "key"}) +
        RespCommandCodec::encode({"SET", "key", "new"}));
    LogCompactor(log_file.path_string()).compact();
    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", "key", "new"}));
}
