#include <gtest/gtest.h>

#include "ArgumentSplitter.h"

#include <optional>
#include <string>

namespace {
CommandArguments split_ok(const std::string& line) {
    const auto arguments = ArgumentSplitter::split(line);
    if (!arguments.has_value()) {
        ADD_FAILURE() << "unexpected split failure for: " << line;
        return {};
    }
    return *arguments;
}
} // namespace

TEST(ArgumentSplitterTest, SplitsBareWords) {
    EXPECT_EQ(split_ok("SET foo bar"), (CommandArguments{"SET", "foo", "bar"}));
}

TEST(ArgumentSplitterTest, KeepsNumericLookingWordsVerbatim) {
    EXPECT_EQ(split_ok("x 007 1.50 1e3 +5 -0"),
              (CommandArguments{"x", "007", "1.50", "1e3", "+5", "-0"}));
}

TEST(ArgumentSplitterTest, CollapsesRunsOfWhitespace) {
    EXPECT_EQ(split_ok("  GET\t\t key \r"), (CommandArguments{"GET", "key"}));
}

TEST(ArgumentSplitterTest, BlankLineProducesNoArguments) {
    EXPECT_EQ(split_ok(""), CommandArguments{});
    EXPECT_EQ(split_ok(" \t "), CommandArguments{});
}

TEST(ArgumentSplitterTest, DoubleQuotesGroupWhitespace) {
    EXPECT_EQ(split_ok("SET \"a key\" \"a value\""), (CommandArguments{"SET", "a key", "a value"}));
}

TEST(ArgumentSplitterTest, EmptyQuotedStringIsOneEmptyArgument) {
    EXPECT_EQ(split_ok("SET k \"\""), (CommandArguments{"SET", "k", ""}));
    EXPECT_EQ(split_ok("SET k ''"), (CommandArguments{"SET", "k", ""}));
}

TEST(ArgumentSplitterTest, DoubleQuotesDecodeEscapes) {
    EXPECT_EQ(split_ok(R"("q\"b\\n\n\r\t\b\a")"),
              (CommandArguments{"q\"b\\n\n\r\t\b\a"}));
}

TEST(ArgumentSplitterTest, DoubleQuotesDecodeHexBytes) {
    EXPECT_EQ(split_ok(R"("\x00\xff\x7A")"), (CommandArguments{Bytes{"\0\xffz", 3}}));
}

TEST(ArgumentSplitterTest, IncompleteHexEscapeFallsBackToLiteralCharacter) {
    EXPECT_EQ(split_ok(R"("\xZZ")"), (CommandArguments{"xZZ"}));
}

TEST(ArgumentSplitterTest, UnknownEscapeDropsBackslash) {
    EXPECT_EQ(split_ok(R"("\q")"), (CommandArguments{"q"}));
}

TEST(ArgumentSplitterTest, SingleQuotesAreLiteralExceptEscapedQuote) {
    EXPECT_EQ(split_ok(R"('a\n"b' 'it\'s')"), (CommandArguments{"a\\n\"b", "it's"}));
}

TEST(ArgumentSplitterTest, BackslashOutsideQuotesIsLiteral) {
    EXPECT_EQ(split_ok(R"(a\nb)"), (CommandArguments{"a\\nb"}));
}

TEST(ArgumentSplitterTest, QuoteCanStartMidWord) {
    EXPECT_EQ(split_ok("ab\"c d\""), (CommandArguments{"abc d"}));
}

TEST(ArgumentSplitterTest, RejectsUnbalancedQuotes) {
    for (const std::string line : {"SET \"k", "SET 'k", "SET \"k\\\"", "SET k \"\\"}) {
        SCOPED_TRACE(line);
        EXPECT_EQ(ArgumentSplitter::split(line), std::nullopt);
    }
}

TEST(ArgumentSplitterTest, RejectsTextImmediatelyAfterClosingQuote) {
    EXPECT_EQ(ArgumentSplitter::split("SET \"a\"b v"), std::nullopt);
    EXPECT_EQ(ArgumentSplitter::split("SET 'a'b v"), std::nullopt);
}

TEST(ArgumentSplitterTest, RawNulByteOutsideQuotesIsPartOfArgument) {
    EXPECT_EQ(split_ok(std::string("a\0b c", 5)), (CommandArguments{Bytes{"a\0b", 3}, "c"}));
}
