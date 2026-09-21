#include "CommandProcessor.h"
#include "Executor.h"
#include "StorageEngine.h"

#include <gtest/gtest.h>

#include <type_traits>
#include <variant>

static_assert(!std::is_default_constructible_v<CommandProcessResult>);

TEST(CommandProcessorTest, ValidCommandProducesProcessedCommand) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const CommandProcessResult result = processor.process("GET \"missing\"");

    ASSERT_TRUE(result.is_success());
    EXPECT_TRUE(std::holds_alternative<GetCommand>(result.processed_command().command));
    EXPECT_THROW(result.error_message(), std::bad_variant_access);
}

TEST(CommandProcessorTest, InvalidInputProducesError) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const CommandProcessResult result = processor.process("UNKNOWN");

    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error_message(), "invalid command");
    EXPECT_THROW(result.processed_command(), std::bad_variant_access);
}

TEST(CommandProcessorTest, InvalidCommandShapeProducesParseError) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const CommandProcessResult result = processor.process("GET");

    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error_message(), "parse failure");
}
