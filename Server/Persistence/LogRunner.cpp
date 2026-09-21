#include "LogRunner.h"
#include "RespCommandCodec.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

LogRunner::LogRunner(const std::string& file_path) : file_path_(file_path) {}

void LogRunner::run_log(CommandProcessor& command_processor) const {
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

            const CommandProcessResult result = command_processor.process_arguments(decoded.arguments);
            if (!result.is_success() || !result.processed_command().mutating_command) {
                const std::string detail = result.is_success() ? "non-mutating command" : result.error_message();
                throw std::runtime_error("Invalid AOF command at byte offset " +
                                         std::to_string(consumed_offset) + ": " + detail);
            }
            if (!result.processed_command().execution_result.success) {
                throw std::runtime_error("AOF command failed at byte offset " +
                                         std::to_string(consumed_offset) + ": " +
                                         result.processed_command().execution_result.message);
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
