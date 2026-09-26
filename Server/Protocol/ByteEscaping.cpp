#include "Protocol/ByteEscaping.h"

#include <cstddef>

namespace {
constexpr char kHexDigits[] = "0123456789abcdef";

// True for bytes that can't appear as-is inside a quoted response value:
// - below 0x20 (space): control characters such as \n, \r, \t, and \0
// - 0x7f and above: DEL and every non-ASCII byte, including UTF-8
// - \\ and \": the characters the quoting itself uses
bool needs_escape(const unsigned char byte) {
    return byte < 0x20 || byte >= 0x7f || byte == '\\' || byte == '"';
}

void append_escape(Bytes& escaped, const unsigned char byte) {
    switch (byte) {
        case '\\': escaped += "\\\\"; return;
        case '"': escaped += "\\\""; return;
        case '\n': escaped += "\\n"; return;
        case '\r': escaped += "\\r"; return;
        case '\t': escaped += "\\t"; return;
        case '\b': escaped += "\\b"; return;
        case '\a': escaped += "\\a"; return;
        default:
            // Written as \xHH, two hex digits. A byte is 8 bits; its top 4 bits
            // (byte >> 4) pick the first digit and its bottom 4 bits (byte & 0x0f)
            // pick the second. For example, 0xe9 becomes "\xe9".
            escaped += "\\x";
            escaped.push_back(kHexDigits[byte >> 4]);
            escaped.push_back(kHexDigits[byte & 0x0f]);
    }
}
}

Bytes escape_bytes(const std::string_view bytes) {
    Bytes escaped;
    escaped.reserve(bytes.size());
    // Copy runs of printable bytes in bulk; values are usually escape-free.
    std::size_t run_start = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto byte = static_cast<unsigned char>(bytes[index]);
        if (!needs_escape(byte)) continue;
        escaped.append(bytes.data() + run_start, index - run_start);
        append_escape(escaped, byte);
        run_start = index + 1;
    }
    escaped.append(bytes.data() + run_start, bytes.size() - run_start);
    return escaped;
}
