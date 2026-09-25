
#include "Persistence/AofCompactor.h"

#include "Commands/Parser.h"
#include "Core/Overloaded.h"
#include "Protocol/RespCommandCodec.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <stdexcept>
#include <unordered_map>
#include <unistd.h>
#include <vector>

namespace {
struct Record { CommandArguments arguments; Command command; bool keep{true}; };

std::vector<Record> read_records(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open AOF log for compaction");
    const Bytes contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::vector<Record> records;
    std::size_t offset = 0;
    while (offset < contents.size()) {
        RespDecodeResult decoded = RespCommandCodec::decode(std::string_view(contents).substr(offset));
        if (decoded.status != RespDecodeStatus::COMPLETE) {
            const std::string detail = decoded.status == RespDecodeStatus::INVALID ? decoded.error : "incomplete RESP record";
            throw std::runtime_error("Malformed AOF at byte offset " + std::to_string(offset) + ": " + detail);
        }
        auto command = Parser::parse_arguments(decoded.arguments);
        if (!command || !is_mutating(*command)) throw std::runtime_error("Invalid AOF command at byte offset " + std::to_string(offset));
        records.push_back({std::move(decoded.arguments), std::move(*command), true});
        offset += decoded.bytes_consumed;
    }
    return records;
}

void write_all(int fd, const Bytes& bytes) {
    const char* data = bytes.data();
    std::size_t remaining = bytes.size();
    while (remaining > 0) {
        const ssize_t written = ::write(fd, data, remaining);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw std::runtime_error("Failed to write compacted AOF: " + std::string(std::strerror(errno)));
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

void replace_atomically(const std::filesystem::path& path, const Bytes& contents) {
    const std::filesystem::path temporary = path.string() + ".compacting";
    int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) throw std::runtime_error("Failed to create compacted AOF: " + std::string(std::strerror(errno)));
    try {
        write_all(fd, contents);
        if (::fsync(fd) != 0) throw std::runtime_error("Failed to sync compacted AOF: " + std::string(std::strerror(errno)));
        if (::close(fd) != 0) throw std::runtime_error("Failed to close compacted AOF: " + std::string(std::strerror(errno)));
        fd = -1;
        if (::rename(temporary.c_str(), path.c_str()) != 0) throw std::runtime_error("Failed to replace compacted AOF: " + std::string(std::strerror(errno)));
        const std::filesystem::path directory = path.has_parent_path() ? path.parent_path() : ".";
        const int directory_fd = ::open(directory.c_str(), O_RDONLY | O_CLOEXEC);
        if (directory_fd < 0) throw std::runtime_error("Failed to open AOF directory");
        const int result = ::fsync(directory_fd);
        const int saved_errno = errno;
        ::close(directory_fd);
        if (result != 0) throw std::runtime_error("Failed to sync AOF directory: " + std::string(std::strerror(saved_errno)));
    } catch (...) {
        if (fd >= 0) ::close(fd);
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
}

AofCompactor::AofCompactor(std::string_view file_path) : file_path_(file_path) {}

void AofCompactor::compact() const {
    if (!std::filesystem::exists(file_path_)) return;

    std::vector<Record> records = read_records(file_path_);
    // Index of the newest surviving record of each kind, per key.
    std::unordered_map<Bytes, std::size_t> latest_set, latest_expire, latest_delete;
    const auto drop_earlier_records = [&](const Bytes& key) {
        for (auto* latest : {&latest_set, &latest_expire, &latest_delete}) {
            if (const auto found = latest->find(key); found != latest->end()) {
                records[found->second].keep = false;
                latest->erase(found);
            }
        }
    };

    for (std::size_t index = 0; index < records.size(); ++index) {
        std::visit(Overloaded{
            // A full value or a delete makes every earlier record for the key irrelevant.
            [&](const OneOf<SetCommand, SetStateCommand> auto& command) {
                drop_earlier_records(command.key);
                latest_set[command.key] = index;
            },
            [&](const DeleteCommand& command) {
                drop_earlier_records(command.key);
                latest_delete[command.key] = index;
            },
            // Raw arithmetic records may depend on the preceding value, so keep everything before them.
            [&](const OneOf<IncrCommand, DecrCommand, IncrByCommand, DecrByCommand> auto& command) {
                latest_set.erase(command.key);
                latest_expire.erase(command.key);
                latest_delete.erase(command.key);
            },
            // Only the newest expiration change for a key matters.
            [&](const OneOf<ExpireCommand, PersistCommand> auto& command) {
                if (const auto found = latest_expire.find(command.key); found != latest_expire.end()) {
                    records[found->second].keep = false;
                }
                latest_expire[command.key] = index;
            },
            [](const OneOf<GetCommand, ExistsCommand, TtlCommand, PttlCommand> auto&) {
                throw std::logic_error("Non-mutating command reached AOF compaction");
            }
        }, records[index].command);
    }
    Bytes output;
    for (const Record& record : records) if (record.keep) output += RespCommandCodec::encode(record.arguments);
    replace_atomically(file_path_, output);
}
