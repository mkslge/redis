#include "AOFLogger.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

AOFLogger::AOFLogger(const std::string& file_path, const AOFFsyncPolicy fsync_policy)
    : file_path_(file_path), fsync_policy_(fsync_policy) {
    fd_ = ::open(file_path_.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd_ < 0) {
        throw_io_error("open");
    }

    if (fsync_policy_ == AOFFsyncPolicy::EVERY_SECOND) {
        try {
            sync_thread_ = std::thread(&AOFLogger::periodic_sync_loop, this);
        } catch (...) {
            ::close(fd_);
            fd_ = -1;
            throw;
        }
    }
}

AOFLogger::~AOFLogger() {
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

void AOFLogger::enqueue(const std::string& log_entry) {
    if (log_entry.empty()) {
        return;
    }

    std::string record = log_entry;
    if (record.back() != '\n') {
        record.push_back('\n');
    }

    std::lock_guard<std::mutex> lock{mutex_};
    if (background_error_) {
        std::rethrow_exception(background_error_);
    }

    write_all(record.data(), record.size());

    switch (fsync_policy_) {
        case AOFFsyncPolicy::ALWAYS:
            sync();
            break;
        case AOFFsyncPolicy::EVERY_SECOND:
            dirty_ = true;
            break;
        case AOFFsyncPolicy::NEVER:
            break;
    }
}

void AOFLogger::write_all(const char* data, std::size_t size) {
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

void AOFLogger::sync() {
    if (::fsync(fd_) != 0) {
        throw_io_error("fsync");
    }
    dirty_ = false;
}

void AOFLogger::periodic_sync_loop() {
    std::unique_lock<std::mutex> lock{mutex_};

    while (!stopping_) {
        sync_condition_.wait_for(lock, std::chrono::seconds(1), [this] {
            return stopping_;
        });

        if (!dirty_) {
            continue;
        }

        try {
            sync();
        } catch (...) {
            background_error_ = std::current_exception();
            return;
        }
    }

    if (dirty_) {
        try {
            sync();
        } catch (...) {
            background_error_ = std::current_exception();
        }
    }
}

void AOFLogger::throw_io_error(const char* operation) const {
    const int error_number = errno;
    throw std::runtime_error(
        "Failed to " + std::string(operation) + " append-only log '" + file_path_ +
        "': " + std::strerror(error_number)
    );
}
