# Persistence

The append-only log (AOF): deciding what to record, writing it, and rebuilding the
keyspace from it on startup.

- **Depends on:** [Core](../Core/README.md), [Commands](../Commands/README.md), and
  [Protocol](../Protocol/README.md) (`RespCommandCodec`).
- **Used by:** `App/CommandProcessor` (`AofRecords`), `Network/Server` (`AofWriter`),
  and `main.cpp` (compaction, then replay).

## Files

| File | Provides | Runs |
|---|---|---|
| `AofRecords.h` / `.cpp` | `aof_records_for`: the RESP records an executed command needs. | Every command |
| `AofWriter.h` / `.cpp` | `AofWriter`: appends records and applies the `fsync` policy. | Every mutation |
| `AofCompactor.h` / `.cpp` | `AofCompactor`: rewrites the log without redundant records. | Once, at startup |
| `AofReplayer.h` / `.cpp` | `AofReplayer`: runs every record through `Parser::parse_arguments` and the `Executor`. | Once, after compaction |
| `AofConfig.h` | The default path, `data/appendonly.aof`, relative to the working directory. | — |
| `CMakeLists.txt` | `SERVER_PERSISTENCE`. | — |

## Record format

Each record is a RESP array of bulk strings, and only four commands ever appear:
`SET`, `DEL`, and two internal commands that clients cannot send:

- `PEXPIREAT key <unix-ms>`: an absolute deadline. Written only for an `EXPIRE`
  whose deadline had already passed, which deletes the key.
- `SETSTATE key value <unix-ms | PERSIST>`: a key's complete state.

Which command writes which records is shown in the root README's
[Durability](../../README.md#durability) diagram.

## Rules

- **A record must not depend on when it is replayed.** Deadlines are absolute and
  `SETSTATE` holds the final value, so never log a relative `EXPIRE` or a raw `INCR`.
- **Write before responding.** `Network/Server` appends a mutation's records before
  queueing its response, and any `AofWriter` exception terminates the server.
- **`fsync` policies:**
  - `ALWAYS` syncs on every append, blocking the event loop.
  - `EVERY_SECOND`, used by `main.cpp`: a background thread syncs about once a
    second, so a crash can lose the last second of acknowledged writes. The
    destructor syncs whatever is left.
  - `NEVER` leaves syncing to the operating system.

  The constructor defaults to `ALWAYS`, which is what most tests use.
- **A background `fsync` failure surfaces only on the next `append`**, which
  rethrows it.
- **Startup is strict, except for an interrupted write.** Compaction and replay
  reject malformed or failing records, and any command other than the four above
  (older servers wrote raw `INCR` and `PERSIST` records; those logs are no longer
  supported). An incomplete *final* record,
  left by a crash during `append`, is dropped with a warning instead, and the replayer
  truncates the file so the next append starts cleanly. Only a final record can be
  incomplete, so this never hides corruption earlier in the file.
- **Compaction is atomic:** it writes `<path>.compacting`, `fsync`s it, renames it
  over the log, then `fsync`s the directory.
- **Compaction runs only at startup**, so the log grows while the server runs (see
  TODO). Its per-key rules:
  - `SET`, `SETSTATE`, or `DEL` makes every earlier record for the key redundant.
  - Only the newest `PEXPIREAT` for a key survives.

## Adding a command

`AofCompactor` fails to compile until the new command is placed in one of its
groups. `AofRecords` needs a change only if the command sets `resulting_state`, and
**nothing fails to compile if that is missed**. See the full recipe in
[Commands](../Commands/README.md#adding-a-command).

## Tests

`testAofPersistence` (23: writer, fsync policies, replay, truncated and malformed
logs) and `testAofCompactor` (13), plus the restart test in `testServerIntegration`.
