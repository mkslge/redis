#include "RespCommandCodec.h"

#include <charconv>
#include <limits>
#include <stdexcept>

namespace {
enum class LineStatus { COMPLETE, INCOMPLETE, INVALID };

struct LineResult {
    LineStatus status;
    std::size_t value{0};
    std::size_t next{0};
    std::string error;
};

LineResult parse_length(std::string_view input, std::size_t offset, char marker) {
    if (offset >= input.size()) return {LineStatus::INCOMPLETE};
    if (input[offset] != marker) return {LineStatus::INVALID, 0, 0, "unexpected RESP type marker"};

    const std::size_t line_end = input.find("\r\n", offset + 1);
    if (line_end == std::string_view::npos) return {LineStatus::INCOMPLETE};
    if (line_end == offset + 1) return {LineStatus::INVALID, 0, 0, "empty RESP length"};

    std::size_t value = 0;
    const char* first = input.data() + offset + 1;
    const char* last = input.data() + line_end;
    const auto parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
        return {LineStatus::INVALID, 0, 0, "invalid RESP length"};
    }
    return {LineStatus::COMPLETE, value, line_end + 2, {}};
}

RespDecodeResult incomplete() { return {RespDecodeStatus::INCOMPLETE}; }
RespDecodeResult invalid(std::string message) {
    return {RespDecodeStatus::INVALID, {}, 0, std::move(message)};
}
}

Bytes RespCommandCodec::encode(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.size() > kMaxArguments) {
        throw std::invalid_argument("RESP command argument count is out of range");
    }

    Bytes output = "*" + std::to_string(arguments.size()) + "\r\n";
    for (const Bytes& argument : arguments) {
        if (argument.size() > kMaxBulkLength || output.size() > kMaxRecordLength - argument.size()) {
            throw std::length_error("RESP command exceeds configured size limit");
        }
        output += "$" + std::to_string(argument.size()) + "\r\n";
        output.append(argument.data(), argument.size());
        output += "\r\n";
    }
    if (output.size() > kMaxRecordLength) throw std::length_error("RESP command exceeds configured size limit");
    return output;
}

RespDecodeResult RespCommandCodec::decode(std::string_view input) {
    const LineResult array = parse_length(input, 0, '*');
    if (array.status == LineStatus::INCOMPLETE) return incomplete();
    if (array.status == LineStatus::INVALID) return invalid(array.error);
    if (array.value == 0 || array.value > kMaxArguments) return invalid("RESP command argument count is out of range");

    CommandArguments arguments;
    arguments.reserve(array.value);
    std::size_t offset = array.next;
    for (std::size_t index = 0; index < array.value; ++index) {
        const LineResult bulk = parse_length(input, offset, '$');
        if (bulk.status == LineStatus::INCOMPLETE) return incomplete();
        if (bulk.status == LineStatus::INVALID) return invalid(bulk.error);
        if (bulk.value > kMaxBulkLength) return invalid("RESP bulk string exceeds configured size limit");
        if (bulk.next > kMaxRecordLength - 2 || bulk.value > kMaxRecordLength - bulk.next - 2) {
            return invalid("RESP command exceeds configured size limit");
        }
        const std::size_t payload_end = bulk.next + bulk.value;
        if (payload_end + 2 > input.size()) return incomplete();
        if (input[payload_end] != '\r' || input[payload_end + 1] != '\n') {
            return invalid("RESP bulk string is missing its terminator");
        }
        arguments.emplace_back(input.substr(bulk.next, bulk.value));
        offset = payload_end + 2;
    }
    return {RespDecodeStatus::COMPLETE, std::move(arguments), offset, {}};
}
