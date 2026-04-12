#include "Persistence/LogRunner.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

LogRunner::LogRunner(const std::string& file_path) : file_path_(file_path) {}

void LogRunner::run_log(CommandProcessor& command_processor) const {
    std::ifstream stream(file_path_);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open append-only log for replay");
    }

    std::string command_line;
    std::size_t line_number = 0;

    while (std::getline(stream, command_line)) {
        ++line_number;

        if (!command_line.empty() && command_line.back() == '\r') {
            command_line.pop_back();
        }

        if (command_line.empty()) {
            continue;
        }

        const CommandProcessResult result = command_processor.process(command_line);
        if (!result.is_success()) {
            std::ostringstream message;
            message << "Failed to replay log line " << line_number << ": " << result.error_message();
            throw std::runtime_error(message.str());
        }

        if (!result.processed_command().execution_result.success) {
            std::ostringstream message;
            message << "Log command failed on line " << line_number << ": "
                    << result.processed_command().execution_result.message;
            throw std::runtime_error(message.str());
        }
    }
}
