#ifndef AOFWRITER_H
#define AOFWRITER_H

#include "Persistence/AofConfig.h"
#include "Core/Bytes.h"

#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <string>
#include <thread>

enum class AofFsyncPolicy {
    ALWAYS,
    EVERY_SECOND,
    NEVER
};

class AofWriter {
public:
    explicit AofWriter(
        const std::string& file_path = std::string(AofConfig::kDefaultPath),
        AofFsyncPolicy fsync_policy = AofFsyncPolicy::ALWAYS
    );
    ~AofWriter();

    AofWriter(const AofWriter&) = delete;
    AofWriter& operator=(const AofWriter&) = delete;

    void append(const Bytes& record);

private:
    void write_all(const char* data, std::size_t size);
    void sync();
    void periodic_sync_loop();
    [[noreturn]] void throw_io_error(const char* operation) const;

    std::string file_path_;
    AofFsyncPolicy fsync_policy_;
    int fd_{-1};
    std::mutex mutex_;
    std::condition_variable sync_condition_;
    std::thread sync_thread_;
    std::uint64_t write_generation_{0};
    std::uint64_t synced_generation_{0};
    bool stopping_{false};
    std::exception_ptr background_error_;
};

#endif //AOFWRITER_H
