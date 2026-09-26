# Network

The server's TCP front end: one thread that accepts clients, reads request lines,
runs each through the command pipeline, persists it, writes the response, and runs
periodic expiration sweeps.

- **Depends on:** [App](../App/README.md) (`CommandProcessor`),
  [Persistence](../Persistence/README.md) (`AofWriter`),
  [Protocol](../Protocol/README.md) (`ResponseFormatter`),
  [Storage](../Storage/README.md) (sweeps), and `Common/Networking` (`SocketIO`).
- **Used by:** `main.cpp`.

## Files

| File | Provides |
|---|---|
| `Server.h` / `.cpp` | `Server`: listening socket, `poll()` event loop, accept, recv/send, sweeps, shutdown. |
| `ClientSession.h` / `.cpp` | `ClientSession`: one client's input and output buffers, line framing, and size limits. No sockets or system calls. |
| `CMakeLists.txt` | `SERVER_NETWORK`. |

The root README's [Event loop](../../README.md#event-loop) and
[One request](../../README.md#one-request) diagrams show the full flow.

## Rules

- **One thread owns everything.** `Server::run()` owns every session and descriptor
  and must be called from one thread only. `Server::stop()` is the only method safe
  to call from another thread: it sets a flag and writes to a wakeup pipe so `poll()`
  returns. Destroy a `Server` only after `run()` has returned.
- **Never block the loop.** All sockets are nonblocking, and every command runs on
  this thread, so a slow operation stalls every client.
- **Per ready client, flush before read.** Each gets at most `kMaxReadPerEvent` and
  `kMaxWritePerEvent` (64 KB each) per loop iteration, so one busy client can't
  starve the others.
- **`ClientSession` limits close the connection:** more than 1 MB of unread input
  (such as one line with no newline) or more than 32 MB of pending output (a client
  that stops reading).
- **Stuck clients are closed; idle ones are not** (`kDefaultClientTimeouts`, overridable
  in the constructor). A client has 10 s to finish a request line once it starts
  (so a client trickling bytes can't hold a connection), and pending output may go
  30 s without any bytes being sent (a client that stopped reading). A client that
  sends nothing, with nothing pending, may stay connected indefinitely, as in Redis.
  Deadlines are checked on the 100 ms sweep tick. `ClientSession` does the timing
  with a `now` parameter, so its tests use fake times.
- **Append before responding.** `process_and_persist` writes a mutation's AOF records
  before its response is queued. Any exception from processing or persisting
  terminates the server.
- **`QUIT` and `EXIT` are handled here, not as commands.** Only the exact strings
  `QUIT`, `quit`, `EXIT`, and `exit` match; the reply is `BYE`, unread input is
  discarded, and the connection closes after the reply is sent. Empty lines are
  ignored.
- **Shutdown closes everyone, including clients not yet accepted.** A client can
  finish connecting before the loop accepts it, so when `run()` exits it first
  accepts whatever is still queued, closes every client, then closes the listening
  socket so later connection attempts are refused.
- **Closing a client:** `close_after_write()` closes once pending output is flushed;
  a peer hangup or socket error closes immediately. Socket errors are logged with
  their `errno` through `socket_io::error_message`.
- **`SIGPIPE` is disabled** on every client socket (`socket_io::configure_for_writes`),
  so writing to a closed peer returns an error instead of killing the process.
- **Port:** `kDefaultPort` is 6380, bound on all interfaces. Passing port 0 lets the
  OS choose one, which tests use; `port()` returns the bound port.
- **Sweeps:** every 100 ms (the constructor's `expiration_sweep_interval`), the loop
  calls `StorageEngine::prune_expired_batch` with up to
  `kExpirationCandidatesPerSweep` (200) keys. `poll()`'s timeout is the time until the
  next sweep.

## Known limitations

- **Running out of file descriptors makes the loop spin on Linux.** When `accept`
  fails with `EMFILE`, the connection stays queued, so `poll()` returns immediately
  and the error is logged on every iteration. macOS drops the connection instead.
- **No signal handling.** Ctrl+C and `docker stop` kill the process without calling
  `stop()`, so clients are reset and `AofWriter`'s final `fsync` is skipped.

## Extending

- Per-connection state and framing belong in `ClientSession`, which stays free of
  system calls so it can be unit tested.
- A new periodic task belongs next to the expiration sweep, and must shorten
  `poll()`'s timeout if it is due sooner.

## Tests

`testServerIntegration` (19, over real sockets) and `testClientSession` (15,
including every timeout rule with fake times).
