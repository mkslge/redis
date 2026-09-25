#ifndef RESPCOMMANDCODEC_H
#define RESPCOMMANDCODEC_H

#include "Core/Bytes.h"

#include <cstddef>
#include <string>
#include <string_view>

enum class RespDecodeStatus { COMPLETE, INCOMPLETE, INVALID };

// Three-state because a stream may simply not have the whole record yet (INCOMPLETE).
struct RespDecodeResult {
    RespDecodeStatus status;
    CommandArguments arguments;
    std::size_t bytes_consumed{0};
    std::string error;
};

class RespCommandCodec {
public:
    static constexpr std::size_t kMaxArguments = 1024;
    static constexpr std::size_t kMaxBulkLength = 512ULL * 1024ULL * 1024ULL;
    static constexpr std::size_t kMaxRecordLength = 512ULL * 1024ULL * 1024ULL + 64ULL * 1024ULL;

    static Bytes encode(const CommandArguments& arguments);
    static RespDecodeResult decode(std::string_view input);
};

#endif
