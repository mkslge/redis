#include "Persistence/AofRecords.h"

#include "Protocol/RespCommandCodec.h"

#include <variant>

namespace {
Bytes encode(const Command& command) {
    return RespCommandCodec::encode(command_arguments(command));
}
}

Bytes aof_records_for(const Command& command, const ExecutionResult& result) {
    if (!result.success || !is_mutating(command) || !result.did_mutate) return {};

    // Commands whose effect is fully described by their own arguments log themselves.
    if (!result.resulting_state) return encode(command);

    // EXPIRE logs its absolute deadline as PEXPIREAT, followed by the key's full state.
    if (std::holds_alternative<ExpireCommand>(command)) {
        return encode(command) + encode(Command{*result.resulting_state});
    }

    // Arithmetic and PERSIST log only the resulting state, so replay never depends
    // on the value or deadline that preceded them.
    return encode(Command{*result.resulting_state});
}
