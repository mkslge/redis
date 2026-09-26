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
    // Skip an optional leading minus sign; `digit` is the index of the first digit.
    std::size_t digit = bytes.front() == '-' ? 1 : 0;
    // A lone "-" has no digits.
    if (digit == bytes.size()) return std::nullopt;
    // Reject leading zeros ("007", "-07") and negative zero ("-0"). A lone "0" is
    // the only number allowed to start with 0.
    if (bytes[digit] == '0' && (digit != 0 || bytes.size() != 1)) return std::nullopt;
    // Everything after the sign must be 0-9, which also rejects '+', spaces, and
    // decimal points.
    for (; digit < bytes.size(); ++digit) {
        if (bytes[digit] < '0' || bytes[digit] > '9') return std::nullopt;
    }
    // The digits are valid; from_chars now fails only if the number doesn't fit in
    // 64 bits.
    std::int64_t value = 0;
    const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != bytes.data() + bytes.size()) return std::nullopt;
    return value;
}

#endif //INTEGER_H
