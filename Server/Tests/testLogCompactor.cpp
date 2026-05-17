#include "Parsing/Parser.h"
#include "Parsing/Tokenizer.h"
#include "Persistence/LogCompactor.h"

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
    Tokenizer tokenizer;
    Parser parser;
    LogCompactor compactor(path, tokenizer, parser);

    compactor.compact();
}

} // namespace

TEST(LogCompactorTest, RemovesOlderSetForSameKey) {
    TempLogFile log_file(
        "older-set",
        "SET \"user\" \"alice\"\n"
        "SET \"counter\" 1\n"
        "SET \"user\" \"bob\"\n");

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        "SET \"counter\" 1\n"
        "SET \"user\" \"bob\"\n");
}

TEST(LogCompactorTest, KeepsLatestExpireForSameKey) {
    TempLogFile log_file(
        "latest-expire",
        "SET \"session\" \"token\"\n"
        "EXPIRE \"session\" 30\n"
        "EXPIRE \"session\" 60\n");

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        "SET \"session\" \"token\"\n"
        "EXPIRE \"session\" 60\n");
}

TEST(LogCompactorTest, DeleteRemovesPriorMutationsForKey) {
    TempLogFile log_file(
        "delete-key",
        "SET \"session\" \"token\"\n"
        "EXPIRE \"session\" 30\n"
        "DEL \"session\"\n"
        "SET \"other\" \"value\"\n");

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        "DEL \"session\"\n"
        "SET \"other\" \"value\"\n");
}

TEST(LogCompactorTest, LeavesIndependentKeysInOriginalOrder) {
    TempLogFile log_file(
        "independent-keys",
        "SET \"a\" 1\n"
        "SET \"b\" 2\n"
        "EXPIRE \"a\" 10\n"
        "SET \"c\" 3\n");

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        "SET \"a\" 1\n"
        "SET \"b\" 2\n"
        "EXPIRE \"a\" 10\n"
        "SET \"c\" 3\n");
}
