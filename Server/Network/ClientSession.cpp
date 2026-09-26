#include "Network/ClientSession.h"

bool ClientSession::append_input(const std::string_view bytes, const Clock::time_point now) {
    if (input_.empty() && !bytes.empty()) partial_line_since_ = now;
    last_input_at_ = now;
    input_.append(bytes.data(), bytes.size());
    return input_.size() <= kMaxInputBuffer;
}

std::optional<std::string> ClientSession::next_line() {
    const std::size_t newline = input_.find('\n');
    if (newline == std::string::npos) return std::nullopt;

    std::string line = input_.substr(0, newline);
    input_.erase(0, newline + 1);  // + 1 also removes the '\n' itself.
    if (!line.empty() && line.back() == '\r') line.pop_back();

    // Whatever remains arrived no earlier than the newline just consumed, so timing it
    // from the latest read errs on the side of giving the client more time.
    if (input_.empty()) partial_line_since_.reset();
    else partial_line_since_ = last_input_at_;
    return line;
}

void ClientSession::discard_input() {
    input_.clear();
    partial_line_since_.reset();
}

bool ClientSession::queue_response(const std::string_view response, const Clock::time_point now) {
    const std::size_t pending = output_.size() - output_offset_;
    if (response.size() > kMaxOutputBuffer - pending) return false;
    // Reclaim already-sent bytes before appending.
    if (output_offset_ > 0) {
        output_.erase(0, output_offset_);
        output_offset_ = 0;
    }
    // The stall clock starts when output goes from nothing pending to something
    // pending; queueing more behind a stalled client doesn't count as progress.
    if (pending == 0 && !response.empty()) output_progress_at_ = now;
    output_.append(response.data(), response.size());
    return true;
}

bool ClientSession::has_pending_output() const {
    return output_offset_ < output_.size();
}

std::string_view ClientSession::pending_output() const {
    return std::string_view(output_).substr(output_offset_);
}

void ClientSession::consume_output(const std::size_t count, const Clock::time_point now) {
    output_offset_ += count;
    if (count > 0) output_progress_at_ = now;
    if (output_offset_ >= output_.size()) {
        output_.clear();
        output_offset_ = 0;
        output_progress_at_.reset();
    }
}

std::optional<std::string_view> ClientSession::timed_out(const Clock::time_point now,
                                                         const Timeouts& timeouts) const {
    if (partial_line_since_ && now - *partial_line_since_ >= timeouts.partial_line) {
        return "request line not finished in time";
    }
    if (output_progress_at_ && now - *output_progress_at_ >= timeouts.write_stall) {
        return "no response bytes accepted in time";
    }
    return std::nullopt;
}

void ClientSession::close_after_write() {
    close_after_write_ = true;
}

bool ClientSession::closing() const {
    return close_after_write_;
}
