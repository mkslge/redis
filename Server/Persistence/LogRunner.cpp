#include "Persistence/LogRunner.h"

#include "Commands/Parser.h"
#include "Protocol/RespCommandCodec.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

LogRunner::LogRunner(const std::string& file_path) : file_path_(file_path) {}

void LogRunner::run_log(Executor& executor) const {
    if (!std::filesystem::exists(file_path_)) return;

    std::ifstream stream(file_path_, std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open append-only log for replay");
    }

    Bytes pending;
    std::size_t consumed_offset = 0;
    char chunk[8192];
    while (stream || !pending.empty()) {
        stream.read(chunk, sizeof(chunk));
        const std::streamsize read = stream.gcount();
        if (read > 0) pending.append(chunk, static_cast<std::size_t>(read));

        while (!pending.empty()) {
            RespDecodeResult decoded = RespCommandCodec::decode(pending);
            if (decoded.status == RespDecodeStatus::INCOMPLETE) break;
            if (decoded.status == RespDecodeStatus::INVALID) {
                throw std::runtime_error("Malformed AOF record at byte offset " +
                                         std::to_string(consumed_offset) + ": " + decoded.error);
            }

            const std::optional<Command> command = Parser::parse_arguments(decoded.arguments);
            if (!command || !is_mutating(*command)) {
                const std::string detail = command ? "non-mutating command" : "parse failure";
                throw std::runtime_error("Invalid AOF command at byte offset " +
                                         std::to_string(consumed_offset) + ": " + detail);
            }
            const ExecutionResult result = executor.execute(*command);
            if (!result.success) {
                throw std::runtime_error("AOF command failed at byte offset " +
                                         std::to_string(consumed_offset) + ": " + result.message);
            }
            pending.erase(0, decoded.bytes_consumed);
            consumed_offset += decoded.bytes_consumed;
        }

        if (read == 0) break;
    }
    if (!stream.eof() && stream.fail()) {
        throw std::runtime_error("Failed while reading append-only log");
    }
    if (!pending.empty()) {
        const RespDecodeResult decoded = RespCommandCodec::decode(pending);
        const std::string detail = decoded.status == RespDecodeStatus::INVALID
            ? decoded.error : "incomplete RESP record";
        throw std::runtime_error("Malformed AOF record at byte offset " +
                                 std::to_string(consumed_offset) + ": " + detail);
    }
}
