# Protocol

Every byte format the server reads or writes: request lines in, response lines
out, and RESP records for the AOF.

- **Depends on:** [Core](../Core/README.md) and [Commands](../Commands/README.md).
  Only `ResponseFormatter` needs Commands, which is why Protocol sits above it.
- **Used by:** `App/CommandProcessor` (splitting), `Network/Server` (formatting), and
  Persistence (RESP encoding and decoding).

## Files

| File | Provides |
|---|---|
| `ArgumentSplitter.h` / `.cpp` | Client request line → `CommandArguments`. |
| `ResponseFormatter.h` / `.cpp` | `Command` + `ExecutionResult` → one response line. |
| `ByteEscaping.h` / `.cpp` | `escape_bytes`: makes a value safe to show inside a response line. |
| `RespCommandCodec.h` / `.cpp` | `CommandArguments` ↔ RESP bytes, for AOF records. |
| `CMakeLists.txt` | `SERVER_PROTOCOL`. |

## Formats

- **Request lines** follow `redis-cli` quoting: see
  [Arguments and values](../../README.md#arguments-and-values) in the root README.
- **Response lines** are the command name plus an optional `field=value`, then
  `\n`, such as `DEL deleted=true`. Errors are `ERROR <message>`. The `BYE` reply to
  `QUIT` comes from `Network/Server`, not from here.
- **RESP** records are arrays of bulk strings, limited to `kMaxArguments` (1024),
  `kMaxBulkLength` (512 MB), and `kMaxRecordLength`. Only the AOF uses RESP today;
  RESP for clients is listed as out of scope in TODO.

## Rules

- **Every response is exactly one line.** Values go through `escape_bytes`, which
  uses the same escapes `ArgumentSplitter` decodes, so anything a response shows can
  be sent back in a request unchanged. `testByteEscaping` checks this round trip
  for every byte.
- **`decode` has three outcomes.** `INCOMPLETE` means "wait for more bytes" and is
  not an error; `bytes_consumed` says how much to drop from the buffer.
- **The two RESP directions report problems differently.** `decode` returns
  `INVALID` with a message, and `encode` throws. These messages are startup or
  internal errors, never sent to clients; client-visible text lives only in
  `Commands/Errors.h`.
- **`encode` throwing terminates the server,** because `Network/Server` treats it as
  a fatal persistence error. It can't happen today only because `ClientSession`'s
  1 MB input limit keeps every command far below the RESP limits.
- **RESP length lines accept leading zeros** (`$03`). That is harmless while the
  server writes every AOF itself, but must be tightened before clients speak RESP.
- **The formatter visits every command**, following Core's `Overloaded` rule. The
  `SETSTATE` case exists only for that: clients can't send `SETSTATE`, so that
  response never happens.

## Adding a response

A new command needs a case in `format_result`, which the compiler enforces. Field
labels are lowercase snake_case; reuse an existing label for the same meaning
(`value`, `deleted`, `exists`, `applied`, `removed`, `ttl`, `ttl_ms`), and pass
any client-supplied bytes through `escape_bytes`.

## Tests

`testArgumentSplitter` (16), `testResponseFormatter` (3, covering every command),
`testByteEscaping` (3), and `testRespCommandCodec` (4). The RESP size limits and
`encode`'s throw paths have no tests yet.
