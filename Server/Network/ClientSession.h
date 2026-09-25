#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// One client's buffered input and output, framed as newline-delimited lines.
// Holds no socket and makes no system calls, so it can be tested on its own.
class ClientSession {
public:
    static constexpr std::size_t kMaxInputBuffer = 1024 * 1024;
    static constexpr std::size_t kMaxOutputBuffer = 32 * 1024 * 1024;

    // Buffers received bytes; returns false once unread input exceeds kMaxInputBuffer.
    bool append_input(std::string_view bytes);
    // Removes and returns the next complete line, without its "\n" or a trailing "\r".
    std::optional<std::string> next_line();
    // Drops any unread input, e.g. after the client asks to quit.
    void discard_input();

    // Queues a response; returns false if pending output would exceed kMaxOutputBuffer.
    bool queue_response(std::string_view response);
    bool has_pending_output() const;
    // The bytes still waiting to be sent.
    std::string_view pending_output() const;
    // Marks bytes as sent after a successful write.
    void consume_output(std::size_t count);

    // Once set, the connection closes after its pending output is flushed.
    void close_after_write();
    bool closing() const;

private:
    std::string input_;
    std::string output_;
    std::size_t output_offset_{0};
    bool close_after_write_{false};
};

#endif //CLIENTSESSION_H
