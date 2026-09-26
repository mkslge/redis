#include "Persistence/AofWriter.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

AofWriter::AofWriter(const std::string& file_path, const AofFsyncPolicy fsync_policy)
    : file_path_(file_path), fsync_policy_(fsync_policy) {
    // Write-only, create the file if missing, always write at the end, and don't
    // leak the descriptor into child processes. 0644: owner can read and write,
    // everyone else can only read.
    fd_ = ::open(file_path_.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd_ < 0) {
        throw_io_error("open");
    }

    if (fsync_policy_ == AofFsyncPolicy::EVERY_SECOND) {
        try {
            sync_thread_ = std::thread(&AofWriter::periodic_sync_loop, this);
        } catch (...) {
            ::close(fd_);
            fd_ = -1;
            throw;
        }
    }
}

AofWriter::~AofWriter() {
    if (sync_thread_.joinable()) {
        {
            std::lock_guard<std::mutex> lock{mutex_};
            stopping_ = true;
        }
        sync_condition_.notify_one();
        sync_thread_.join();
    }

    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void AofWriter::append(const Bytes& record) {
    if (record.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock{mutex_};
    if (background_error_) {
        std::rethrow_exception(background_error_);
    }

    write_all(record.data(), record.size());

    switch (fsync_policy_) {
        case AofFsyncPolicy::ALWAYS:
            sync();
            break;
        case AofFsyncPolicy::EVERY_SECOND:
            ++write_generation_;
            break;
        case AofFsyncPolicy::NEVER:
            break;
    }
}

void AofWriter::write_all(const char* data, std::size_t size) {
    while (size > 0) {
        const ssize_t bytes_written = ::write(fd_, data, size);
        if (bytes_written < 0 && errno == EINTR) {
            continue;
        }
        if (bytes_written <= 0) {
            if (bytes_written == 0) {
                errno = EIO;
            }
            throw_io_error("write");
        }

        data += bytes_written;
        size -= static_cast<std::size_t>(bytes_written);
    }
}

void AofWriter::sync() {
    if (::fsync(fd_) != 0) {
        throw_io_error("fsync");
    }
}

void AofWriter::periodic_sync_loop() {
    std::unique_lock<std::mutex> lock{mutex_};

    while (true) {
        sync_condition_.wait_for(lock, std::chrono::seconds(1), [this] {
            return stopping_;
        });

        if (synced_generation_ == write_generation_) {
            if (stopping_) return;
            continue;
        }

        const std::uint64_t generation_to_sync = write_generation_;
        lock.unlock();

        try {
            sync();
        } catch (...) {
            lock.lock();
            background_error_ = std::current_exception();
            return;
        }
        lock.lock();
        synced_generation_ = generation_to_sync;
        if (stopping_ && synced_generation_ == write_generation_) return;
    }
}

void AofWriter::throw_io_error(const char* operation) const {
    const int error_number = errno;
    throw std::runtime_error(
        "Failed to " + std::string(operation) + " append-only log '" + file_path_ +
        "': " + std::strerror(error_number)
    );
}
