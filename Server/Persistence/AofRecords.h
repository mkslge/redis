#ifndef AOFRECORDS_H
#define AOFRECORDS_H

#include "Commands/Command.h"
#include "Commands/ExecutionResult.h"
#include "Core/Bytes.h"

// Returns the RESP-encoded AOF records for one executed command, or empty bytes
// when nothing needs to be persisted (read-only, failed, or no-op commands).
Bytes aof_records_for(const Command& command, const ExecutionResult& result);

#endif //AOFRECORDS_H
