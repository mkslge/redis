#ifndef AOFLOGGER_H
#define AOFLOGGER_H

#include "LogConfig.h"
#include "Bytes.h"

#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <string>
#include <thread>

enum class AOFFsyncPolicy {
    ALWAYS,
    EVERY_SECOND,
    NEVER
};

class AOFLogger {
public:
    explicit AOFLogger(
        const std::string& file_path = std::string(LogConfig::kDefaultAofPath),
        AOFFsyncPolicy fsync_policy = AOFFsyncPolicy::ALWAYS
    );
    ~AOFLogger();

    AOFLogger(const AOFLogger&) = delete;
    AOFLogger& operator=(const AOFLogger&) = delete;

    void append_record(const Bytes& record);

private:
    void write_all(const char* data, std::size_t size);
    void sync();
    void periodic_sync_loop();
    [[noreturn]] void throw_io_error(const char* operation) const;

    std::string file_path_;
    AOFFsyncPolicy fsync_policy_;
    int fd_{-1};
    std::mutex mutex_;
    std::condition_variable sync_condition_;
    std::thread sync_thread_;
    std::uint64_t write_generation_{0};
    std::uint64_t synced_generation_{0};
    bool stopping_{false};
    std::exception_ptr background_error_;
};

#endif //AOFLOGGER_H
