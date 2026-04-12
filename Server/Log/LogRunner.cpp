#include "Log/LogRunner.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

LogRunner::LogRunner(const std::string& file_path) : file_path_(file_path) {}

const std::string& LogRunner::file_path() const {
    return file_path_;
}

void LogRunner::run_log(CommandHandler& command_handler) const {
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

        const CommandResponse result = command_handler.process_command(command_line);
        if (!result.success) {
            std::ostringstream message;
            message << "Failed to replay log line " << line_number << ": " << result.error_message;
            throw std::runtime_error(message.str());
        }

        if (!result.execution_result.success) {
            std::ostringstream message;
            message << "Log command failed on line " << line_number << ": "
                    << result.execution_result.message;
            throw std::runtime_error(message.str());
        }
    }
}
