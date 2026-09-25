#ifndef INTEGER_H
#define INTEGER_H

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

// Parses a csigned 64-bit decimal
inline std::optional<std::int64_t> parse_integer(const std::string_view bytes) {
    if (bytes.empty()) return std::nullopt;
    std::size_t digit = bytes.front() == '-' ? 1 : 0;
    if (digit == bytes.size()) return std::nullopt;
    if (bytes[digit] == '0' && (digit != 0 || bytes.size() != 1)) return std::nullopt;
    for (; digit < bytes.size(); ++digit) {
        if (bytes[digit] < '0' || bytes[digit] > '9') return std::nullopt;
    }
    std::int64_t value = 0;
    const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != bytes.data() + bytes.size()) return std::nullopt;
    return value;
}

#endif //INTEGER_H
