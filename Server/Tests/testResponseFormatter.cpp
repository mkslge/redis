#include "Protocol/ResponseFormatter.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
ExecutionResult with_payload(ExecutionPayload payload) {
    return {.success = true, .payload = std::move(payload)};
}
} // namespace

TEST(ResponseFormatterTest, FormatsEveryCommandResponse) {
    const ExpireCommand::TimePoint deadline{};
    for (const auto& [command, result, expected] :
         std::vector<std::tuple<Command, ExecutionResult, std::string>>{
             {GetCommand{"k"}, with_payload(Value("v")), "GET value=\"v\"\n"},
             {GetCommand{"k"}, with_payload(std::monostate{}), "GET\n"},
             {SetCommand{"k", "v"}, with_payload(Value("v")), "SET value=\"v\"\n"},
             {DeleteCommand{"k"}, with_payload(true), "DEL deleted=true\n"},
             {ExistsCommand{"k"}, with_payload(false), "EXISTS exists=false\n"},
             {ExpireCommand{"k", deadline}, with_payload(true), "EXPIRE applied=true\n"},
             {PersistCommand{"k"}, with_payload(false), "PERSIST removed=false\n"},
             {TtlCommand{"k"}, with_payload(std::int64_t{-2}), "TTL ttl=-2\n"},
             {PttlCommand{"k"}, with_payload(std::int64_t{1500}), "PTTL ttl_ms=1500\n"},
             {IncrCommand{"k"}, with_payload(std::int64_t{1}), "INCR value=1\n"},
             {DecrCommand{"k"}, with_payload(std::int64_t{-1}), "DECR value=-1\n"},
             {IncrByCommand{"k", "5"}, with_payload(std::int64_t{5}), "INCRBY value=5\n"},
             {DecrByCommand{"k", "5"}, with_payload(std::int64_t{-5}), "DECRBY value=-5\n"},
             {SetStateCommand{"k", "v", std::nullopt}, with_payload(std::monostate{}), "SETSTATE\n"}}) {
        EXPECT_EQ(ResponseFormatter::format_result(command, result), expected);
    }
}

TEST(ResponseFormatterTest, ExecutionFailureIsFormattedAsError) {
    const ExecutionResult failed{.success = false, .message = "value is not an integer or out of range"};
    EXPECT_EQ(ResponseFormatter::format_result(IncrCommand{"k"}, failed),
              "ERROR value is not an integer or out of range\n");
}
