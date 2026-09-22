#include "ResponseFormatter.h"

#include "Value.h"

#include <string>
#include <type_traits>

namespace {
template<class>
inline constexpr bool always_false = false;

std::string format_value(const Value& value) {
    return '"' + value.bytes() + '"';
}
}

std::string ResponseFormatter::format_error(const std::string& error_message) {
    return "ERROR " + error_message + "\n";
}

std::string ResponseFormatter::format_result(const Command& command, const ExecutionResult& result) {
    if (!result.success) return format_error(result.message);

    return std::visit([&result](const auto& concrete) -> std::string {
        using Type = std::decay_t<decltype(concrete)>;
        if constexpr (std::is_same_v<Type, GetCommand>) {
            std::string response = "GET";
            if (std::holds_alternative<Value>(result.payload)) {
                response += " value=" + format_value(std::get<Value>(result.payload));
            }
            return response + "\n";
        }
        else if constexpr (std::is_same_v<Type, SetCommand>)
            return "SET value=" + format_value(std::get<Value>(result.payload)) + "\n";
        else if constexpr (std::is_same_v<Type, DeleteCommand>)
            return "DELETE deleted=" + std::string(std::get<bool>(result.payload) ? "true" : "false") + "\n";
        else if constexpr (std::is_same_v<Type, ExistsCommand>)
            return "EXISTS exists=" + std::string(std::get<bool>(result.payload) ? "true" : "false") + "\n";
        else if constexpr (std::is_same_v<Type, ExpireCommand>)
            return "EXPIRE applied=" + std::string(std::get<bool>(result.payload) ? "true" : "false") + "\n";
        else if constexpr (std::is_same_v<Type, TtlCommand>)
            return "TTL ttl=" + std::to_string(std::get<std::int64_t>(result.payload)) + "\n";
        else if constexpr (std::is_same_v<Type, PttlCommand>)
            return "PTTL ttl_ms=" + std::to_string(std::get<std::int64_t>(result.payload)) + "\n";
        else if constexpr (std::is_same_v<Type, PersistCommand>)
            return "PERSIST removed=" + std::string(std::get<bool>(result.payload) ? "true" : "false") + "\n";
        else static_assert(always_false<Type>, "Response formatter missing command alternative");
    }, command);
}
