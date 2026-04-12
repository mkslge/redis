#include "Log/AOFLogger.h"

#include <stdexcept>

AOFLogger::AOFLogger(const std::string& file_path)
    : stream_(file_path, std::ios::out | std::ios::app) {
    if (!stream_.is_open()) {
        throw std::runtime_error("Failed to open append-only log");
    }
}

void AOFLogger::enqueue(const std::string& log_entry) {
    if (log_entry.empty()) {
        return;
    }

    stream_ << log_entry;
    if (log_entry.back() != '\n') {
        stream_ << '\n';
    }
    stream_.flush();
}
