# App

Turns one client command line into everything the network layer needs to answer
it: the parsed command, its execution result, and the AOF bytes to persist.

```text
line -> ArgumentSplitter -> Parser::parse_request -> Executor -> aof_records_for -> ProcessedCommand
```

- **Depends on:** [Protocol](../Protocol/README.md) (`ArgumentSplitter`),
  [Commands](../Commands/README.md) (`Parser`, `Executor`, `Errors.h`), and
  [Persistence](../Persistence/README.md) (`aof_records_for`).
- **Used by:** `Network/Server` and `main.cpp`, which builds the `CommandProcessor`.

## Files

| File | Provides |
|---|---|
| `CommandProcessor.h` / `.cpp` | `CommandProcessor::process`, plus its result types `CommandProcessResult`, `ProcessedCommand`, and `ProcessError`. |
| `CMakeLists.txt` | `SERVER_APP`. |

## Rules

- **App decides, Network acts.** `process` returns the AOF bytes but never writes
  them; `Network/Server` appends them before queueing the response. App never
  touches sockets or files.
- **Two kinds of failure, in two places.**
  - `CommandProcessResult` is a failure only when the line never became a command:
    unbalanced quotes (from `ArgumentSplitter`) or a `ParseError`.
  - A command that ran but failed, such as `INCR` on a non-integer, is a *success*
    here, with `execution_result.success == false`.
- **`ProcessedCommand` fields:**
  - `command` and `execution_result` are what `ResponseFormatter` needs.
  - `aof_record` holds every RESP record to append, and is empty when nothing
    should be persisted. `should_log` is exactly `!aof_record.empty()`.
  - `mutating_command` is `is_mutating(command)`; only tests read it.
- **No state of its own.** `CommandProcessor` holds only an `Executor&`, and
  `process` is `const`. It runs on the event-loop thread with everything else.
- **Client lines only.** AOF replay does not come through here; `AofReplayer` calls
  `Parser::parse_arguments` and the `Executor` directly. `QUIT` and `EXIT` never
  reach here either; `Network/Server` handles them.
- **Error text comes from `Commands/Errors.h`**, including the unbalanced-quotes
  message.

## Tests

`testCommandProcessor` (10): valid and invalid lines, error messages, AOF records
for successful and failed numeric commands, and the byte-exact regression tests
for keys such as `007` and `1.50`. AOF record contents are also covered in
`testAofPersistence`.
