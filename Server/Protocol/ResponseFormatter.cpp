#include "Protocol/ResponseFormatter.h"

#include "Core/Overloaded.h"
#include "Protocol/ByteEscaping.h"
#include "Core/Value.h"

#include <string>
#include <string_view>

namespace {
// Every response is one line: the command name, then an optional "field=value".
std::string response_line(const std::string_view name, const std::string& field = {}) {
    std::string line(name);
    if (!field.empty()) line += " " + field;
    return line + "\n";
}

// Values are escaped so one response always stays on one line.
std::string value_field(const ExecutionPayload& payload) {
    return "value=\"" + escape_bytes(std::get<Value>(payload).bytes()) + "\"";
}

std::string bool_field(const std::string_view label, const ExecutionPayload& payload) {
    return std::string(label) + "=" + (std::get<bool>(payload) ? "true" : "false");
}

std::string int_field(const std::string_view label, const ExecutionPayload& payload) {
    return std::string(label) + "=" + std::to_string(std::get<std::int64_t>(payload));
}
}

std::string ResponseFormatter::format_error(const std::string& error_message) {
    return "ERROR " + error_message + "\n";
}

std::string ResponseFormatter::format_result(const Command& command, const ExecutionResult& result) {
    if (!result.success) return format_error(result.message);
    const ExecutionPayload& payload = result.payload;

    return std::visit(Overloaded{
        [&](const GetCommand& c) {
            // A missing key has no value field.
            return response_line(c.name, std::holds_alternative<Value>(payload) ? value_field(payload) : "");
        },
        [&](const SetCommand& c) { return response_line(c.name, value_field(payload)); },
        [&](const DeleteCommand& c) { return response_line(c.name, bool_field("deleted", payload)); },
        [&](const ExistsCommand& c) { return response_line(c.name, bool_field("exists", payload)); },
        [&](const ExpireCommand& c) { return response_line(c.name, bool_field("applied", payload)); },
        [&](const PersistCommand& c) { return response_line(c.name, bool_field("removed", payload)); },
        [&](const TtlCommand& c) { return response_line(c.name, int_field("ttl", payload)); },
        [&](const PttlCommand& c) { return response_line(c.name, int_field("ttl_ms", payload)); },
        [&](const OneOf<IncrCommand, DecrCommand, IncrByCommand, DecrByCommand> auto& c) {
            return response_line(c.name, int_field("value", payload));
        },
        [&](const SetStateCommand& c) { return response_line(c.name); }
    }, command);
}
