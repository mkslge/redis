#include "Persistence/AofRecords.h"

#include "Protocol/RespCommandCodec.h"


namespace {
Bytes encode(const Command& command) {
    return RespCommandCodec::encode(command_arguments(command));
}
}

Bytes aof_records_for(const Command& command, const ExecutionResult& result) {
    if (!result.success || !is_mutating(command) || !result.did_mutate) return {};

    // Commands whose effect is fully described by their own arguments log themselves:
    // SET, DEL, and an EXPIRE whose deadline had already passed, which deleted the key
    // and is logged as PEXPIREAT so replay deletes it too.
    if (!result.resulting_state) return encode(command);

    // EXPIRE, PERSIST, and the arithmetic commands log only the key's resulting state,
    // so replay never depends on the value or deadline that preceded them.
    return encode(Command{*result.resulting_state});
}
