#include "Protocol/ArgumentSplitter.h"
#include "Protocol/ByteEscaping.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

TEST(ByteEscapingTest, EscapesLikeRedisCli) {
    for (const auto& [input, expected] : std::vector<std::pair<Bytes, Bytes>>{
             {"plain text 123", "plain text 123"},
             {"", ""},
             {R"(say "hi")", R"(say \"hi\")"},
             {R"(back\slash)", R"(back\\slash)"},
             {"\n\r\t\b\a", R"(\n\r\t\b\a)"},
             {Bytes{"\0", 1}, R"(\x00)"},
             {"\x1f\x7f\xff", R"(\x1f\x7f\xff)"},
             {"\xc3\xa9", R"(\xc3\xa9)"}}) {
        EXPECT_EQ(escape_bytes(input), expected);
    }
}

TEST(ByteEscapingTest, OutputNeverBreaksAResponseLine) {
    for (int value = 0; value < 256; ++value) {
        const Bytes escaped = escape_bytes(Bytes(1, static_cast<char>(value)));
        EXPECT_EQ(escaped.find('\n'), Bytes::npos) << value;
        EXPECT_EQ(escaped.find('\r'), Bytes::npos) << value;
    }
}

// Whatever a response shows inside quotes can be sent back in a request unchanged.
TEST(ByteEscapingTest, ArgumentSplitterDecodesEveryEscapedByte) {
    std::vector<Bytes> inputs;
    for (int value = 0; value < 256; ++value) inputs.emplace_back(1, static_cast<char>(value));
    inputs.push_back("mixed \"quotes\" \\ \n\r\t\b\a \xc3\xa9 end");
    inputs.push_back(Bytes{"\0\xff\0", 3});
    inputs.push_back(R"(\x41 is not an escape here)");

    for (const Bytes& input : inputs) {
        const auto arguments = ArgumentSplitter::split("\"" + escape_bytes(input) + "\"");
        ASSERT_TRUE(arguments.has_value()) << escape_bytes(input);
        EXPECT_EQ(*arguments, CommandArguments{input}) << escape_bytes(input);
    }
}
