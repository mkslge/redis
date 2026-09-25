#include "Protocol/RespCommandCodec.h"

#include <gtest/gtest.h>

TEST(RespCommandCodecTest, RoundTripsArbitraryBytes) {
    const CommandArguments arguments{"SET", Bytes{"k\0\r\n", 4}, Bytes{"\0\xff\r\n", 4}};
    const Bytes encoded = RespCommandCodec::encode(arguments);
    const RespDecodeResult decoded = RespCommandCodec::decode(encoded);
    ASSERT_EQ(decoded.status, RespDecodeStatus::COMPLETE);
    EXPECT_EQ(decoded.arguments, arguments);
    EXPECT_EQ(decoded.bytes_consumed, encoded.size());
}

TEST(RespCommandCodecTest, ConsumesOnlyFirstConcatenatedRecord) {
    const Bytes first = RespCommandCodec::encode({"DEL", "a"});
    const Bytes second = RespCommandCodec::encode({"DEL", "b"});
    const RespDecodeResult decoded = RespCommandCodec::decode(first + second);
    ASSERT_EQ(decoded.status, RespDecodeStatus::COMPLETE);
    EXPECT_EQ(decoded.arguments, CommandArguments({"DEL", "a"}));
    EXPECT_EQ(decoded.bytes_consumed, first.size());
}

TEST(RespCommandCodecTest, EveryProperPrefixIsIncomplete) {
    const Bytes encoded = RespCommandCodec::encode({"SET", "key", "value\r\n"});
    for (std::size_t length = 0; length < encoded.size(); ++length) {
        EXPECT_EQ(RespCommandCodec::decode(std::string_view(encoded).substr(0, length)).status,
                  RespDecodeStatus::INCOMPLETE) << length;
    }
}

TEST(RespCommandCodecTest, RejectsMalformedLengthAndTerminator) {
    EXPECT_EQ(RespCommandCodec::decode("*x\r\n").status, RespDecodeStatus::INVALID);
    EXPECT_EQ(RespCommandCodec::decode("*1\r\n$1\r\naXX").status, RespDecodeStatus::INVALID);
    EXPECT_EQ(RespCommandCodec::decode("*0\r\n").status, RespDecodeStatus::INVALID);
}
