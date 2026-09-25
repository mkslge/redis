#include "Network/ClientSession.h"

bool ClientSession::append_input(const std::string_view bytes) {
    input_.append(bytes.data(), bytes.size());
    return input_.size() <= kMaxInputBuffer;
}

std::optional<std::string> ClientSession::next_line() {
    const std::size_t newline = input_.find('\n');
    if (newline == std::string::npos) return std::nullopt;

    std::string line = input_.substr(0, newline);
    input_.erase(0, newline + 1);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

void ClientSession::discard_input() {
    input_.clear();
}

bool ClientSession::queue_response(const std::string_view response) {
    const std::size_t pending = output_.size() - output_offset_;
    if (response.size() > kMaxOutputBuffer - pending) return false;
    // Reclaim already-sent bytes before appending.
    if (output_offset_ > 0) {
        output_.erase(0, output_offset_);
        output_offset_ = 0;
    }
    output_.append(response.data(), response.size());
    return true;
}

bool ClientSession::has_pending_output() const {
    return output_offset_ < output_.size();
}

std::string_view ClientSession::pending_output() const {
    return std::string_view(output_).substr(output_offset_);
}

void ClientSession::consume_output(const std::size_t count) {
    output_offset_ += count;
    if (output_offset_ >= output_.size()) {
        output_.clear();
        output_offset_ = 0;
    }
}

void ClientSession::close_after_write() {
    close_after_write_ = true;
}

bool ClientSession::closing() const {
    return close_after_write_;
}
