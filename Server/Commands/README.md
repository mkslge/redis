# Commands

What a command means and does: arguments are parsed into a `Command`, and the
`Executor` runs it against storage to produce an `ExecutionResult`.

```text
CommandArguments -> Parser -> Command -> Executor (+ StorageEngine) -> ExecutionResult
```

- **Depends on:** [Core](../Core/README.md) and [Storage](../Storage/README.md).
- **Used by:** Protocol (formats results), Persistence (AOF records, replay,
  compaction), and App (runs the whole pipeline).

## Files

| File | Provides |
|---|---|
| `Command.h` / `.cpp` | One struct per command (`name`, `mutating`, fields), the `Command` variant, `is_mutating`, and `command_arguments` (a command's AOF form). |
| `Parser.h` / `.cpp` | `parse_request` for client commands and `parse_arguments` for AOF replay. |
| `Executor.h` / `.cpp` | `Executor`: runs one `Command` against `StorageEngine`. |
| `ExecutionResult.h` | `ExecutionResult`: what a command produced. |
| `Errors.h` | Every error message a client can receive. |
| `CMakeLists.txt` | `SERVER_COMMANDS`. |

## Rules

- **Two parsers accept different commands.** `parse_request` accepts relative
  `EXPIRE` but not `PEXPIREAT` or `SETSTATE`; `parse_arguments` accepts the reverse.
  Both share the table of key-based commands, and AOF replay rejects the read-only
  ones afterwards.
- **`EXPIRE` becomes an absolute deadline at parse time**, using `Clock::now()`. The
  parser is therefore not pure for `EXPIRE`, and its tests check time bounds.
- **Where a number is parsed decides the kind of error.** `EXPIRE` seconds are parsed
  in the Parser, so a bad value is a parse error. `INCRBY`/`DECRBY` amounts stay
  `Bytes` until Storage validates them, so a bad amount is an execution error. AOF
  deadlines use `from_chars` because the server wrote them, not a client.
- **`ExecutionResult` fields:**
  - `success == false` is a runtime error shown to the user, with `message` set.
  - `did_mutate == false` means a mutating command changed nothing (such as `DEL` on
    a missing key), so no AOF record is written.
  - `payload`'s type is fixed per command: `Value`, `bool`, `int64_t`, or none.
  - `resulting_state` is set only by `EXPIRE`, `PERSIST`, and the arithmetic commands.
- **Error messages are part of the wire protocol.** Use the constants in `Errors.h`;
  never write message text inline. Tests assert the exact strings.
- **`TTL` rounds** milliseconds to the nearest second, halves up, in the Executor.
  `PTTL` does not round.
- **The Executor holds no state** beyond its storage reference; thread safety comes
  from `StorageEngine`.

## Adding a command

1. `Command.h`: add a struct with `name`, `mutating`, and its fields, and add it to
   the `Command` variant.
2. `Parser.cpp`: add `key_only<T>()` or `key_and_amount<T>()` to `kKeyCommands`, or
   write a parse function like `parse_expire`. **Nothing fails to compile if you skip
   this step**; the command is simply unreachable.
3. `Command.cpp`: add the struct to a `command_arguments` case.
4. `Executor.h` / `.cpp`: add an `execute_command` overload.
5. Outside this module: `Protocol/ResponseFormatter` and `Persistence/AofCompactor`.
   `Persistence/AofRecords` needs a change only if the command sets
   `resulting_state`, and **nothing fails to compile if it's missed**.
6. Tests (`testParser`, `testExecutor`, `testResponseFormatter`) and the root
   README's command list.

The compiler enforces step 3, step 4, and the `ResponseFormatter` and
`AofCompactor` parts of step 5. It does not enforce steps 2 or 6, or `AofRecords`.

## Tests

`testParser` and `testExecutor`, plus the full client path in
`testCommandProcessor`.
