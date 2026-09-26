#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// One client's buffered input and output, framed as newline-delimited lines.
// Holds no socket and makes no system calls, so it can be tested on its own:
// methods that depend on time take `now` instead of reading a clock.
class ClientSession {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::size_t kMaxInputBuffer = 1024 * 1024;
    static constexpr std::size_t kMaxOutputBuffer = 32 * 1024 * 1024;

    // How long a client may leave the connection stuck before it is closed.
    // There is deliberately no idle timeout: a client sending nothing, with nothing
    // pending, may stay connected indefinitely, as in Redis.
    struct Timeouts {
        // Time to finish a request line once its first byte has arrived.
        Clock::duration partial_line;
        // Time pending output may go without any of it being sent.
        Clock::duration write_stall;
    };

    // Buffers received bytes; returns false once unread input exceeds kMaxInputBuffer.
    bool append_input(std::string_view bytes, Clock::time_point now);
    // Removes and returns the next complete line, without its "\n" or a trailing "\r".
    std::optional<std::string> next_line();
    // Drops any unread input, e.g. after the client asks to quit.
    void discard_input();

    // Queues a response; returns false if pending output would exceed kMaxOutputBuffer.
    bool queue_response(std::string_view response, Clock::time_point now);
    bool has_pending_output() const;
    // The bytes still waiting to be sent.
    std::string_view pending_output() const;
    // Marks bytes as sent after a successful write.
    void consume_output(std::size_t count, Clock::time_point now);

    // Why the connection should be closed for being stuck, or nullopt if it isn't.
    std::optional<std::string_view> timed_out(Clock::time_point now, const Timeouts& timeouts) const;

    // Once set, the connection closes after its pending output is flushed.
    void close_after_write();
    bool closing() const;

private:
    std::string input_;
    std::string output_;
    std::size_t output_offset_{0};
    bool close_after_write_{false};

    // When the unfinished line now in input_ started arriving; empty when input_ is.
    std::optional<Clock::time_point> partial_line_since_;
    Clock::time_point last_input_at_{};
    // When pending output last made progress (was queued or partly sent); empty when
    // there is no pending output.
    std::optional<Clock::time_point> output_progress_at_;
};

#endif //CLIENTSESSION_H
