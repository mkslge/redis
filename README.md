# My implementation of Redis

The project currently includes:

- a single-threaded, `poll()`-based TCP server with a simple newline-delimited text protocol
- a CLI client for interactive testing
- byte-string keys and values that preserve the supplied representation
- key expiration with lazy pruning and periodic event-loop sweeps
- binary-safe RESP-framed append-only logging for durable mutation replay
- unit and integration tests for the parser, runtime, persistence, networking, and client

## Current Status

This is a working educational systems project, not a Redis-compatible clone.

Implemented today:

- `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `PTTL`, `PERSIST`,
  `INCR`, `DECR`, `INCRBY`, `DECRBY`, `QUIT`, `EXIT`
- append-only log writes for mutating commands
- log replay on server startup
- multiple concurrent client sessions on one nonblocking event-loop thread
- buffered partial reads and writes
- absolute expiration timestamps in the AOF
- separate CMake projects for the server and client

Not implemented:

- RESP / Redis protocol compatibility
- transactions, replication, snapshots, pub/sub, or clustering
- advanced data structures beyond scalar values

## Supported Commands

Commands are sent as one line each.

- `SET <key> <value>`
- `GET <key>`
- `DEL <key>`
- `EXISTS <key>`
- `EXPIRE <key> <seconds>`
- `TTL <key>`
- `PTTL <key>`
- `PERSIST <key>`
- `INCR <key>`
- `DECR <key>`
- `INCRBY <key> <amount>`
- `DECRBY <key> <amount>`
- `QUIT`
- `EXIT`

### Arguments and values

Every argument is decoded as a byte string, and keys and values are stored exactly
as sent: `GET 007` reads key `007`, not `7`. Only commands that need numbers parse
them: `EXPIRE` seconds and `INCRBY`/`DECRBY` amounts must be canonical signed
64-bit decimals (no `+`, leading zeros, `-0`, or decimals).

Arguments follow `redis-cli` quoting rules:

- unquoted words are split on whitespace
- `"..."` supports the escapes `\"`, `\\`, `\n`, `\r`, `\t`, `\b`, `\a`, and `\xHH`
- `'...'` is literal except for `\'`
- a closing quote must be followed by whitespace or the end of the line

The line protocol still cannot return arbitrary bytes in responses; full
end-to-end binary safety requires a length-prefixed protocol such as RESP.

Examples:

```text
SET name mark
SET "full name" "Mark S"
SET code 00123
SET bytes "\x00\xff"
GET name
EXISTS code
EXPIRE name 30
DEL code
QUIT
```

### Integer commands

Integer commands interpret stored byte strings as signed 64-bit decimal integers
and write the result back as canonical decimal bytes. Missing keys start at zero.
Existing expiration deadlines are retained. Invalid integers and arithmetic
overflow return an error without changing the key. `INCRBY` and `DECRBY` accept
negative amounts. A malformed amount is an execution error, not a parse error.

### Expiration

`EXPIRE` accepts a relative lifetime in seconds. The server immediately converts
it to an absolute deadline, and the AOF stores that deadline as an internal
`PEXPIREAT` command. Restarting the server therefore does not grant the key a
fresh lifetime.

- `TTL` returns the remaining lifetime in whole seconds.
- `PTTL` returns the remaining lifetime in milliseconds.
- Both return `-1` when the key exists without an expiration and `-2` when the
  key does not exist.
- `PERSIST` removes an existing expiration.

Expired keys are removed when accessed and by periodic sweeps performed by the
server event loop.

## Wire Protocol

The server uses a simple newline-delimited text protocol rather than RESP.

Example responses:

```text
SET value="C++"
GET value="C++"
EXISTS exists=true
EXPIRE applied=true
TTL ttl=29
PTTL ttl_ms=...
PERSIST removed=true
INCR value=1
INCRBY value=6
DEL deleted=true
BYE
ERROR unknown command
ERROR wrong number of arguments for 'get' command
```

## Persistence

Mutating commands are appended to:

`data/appendonly.aof`

On startup, the server replays that file before accepting client traffic.

The AOF is a sequence of RESP arrays whose bulk strings are length-prefixed, so
keys and values containing null bytes, CRLF, quotes, or arbitrary binary data
round-trip exactly. It is not intended for manual editing. Startup rejects
malformed commands and truncated records instead of silently discarding them.
Successful integer commands log an internal `SETSTATE` record containing the
resulting value and its absolute expiration deadline. Successful `PERSIST` and
future `EXPIRE` operations also record their resulting state. This prevents
replay from losing a key whose expiration was removed or extended before the
server stopped.

The default persistence policy is `EVERY_SECOND`. A successful mutation is
appended before its response is queued, while a helper thread calls `fsync()`
approximately once per second. This improves throughput, but a process or
machine crash can lose roughly the most recent second of acknowledged writes.
Clean logger shutdown performs a final synchronization.

## Concurrency Model

The server handles multiple simultaneous clients with one server event-loop
thread:

- `poll()` watches the listening socket, a shutdown wakeup pipe, and all clients
- accepted client sockets are nonblocking
- every client has its own input and output buffers
- readable clients can submit complete newline-delimited commands
- writable clients flush queued responses across as many partial writes as needed
- per-event read and write budgets prevent one busy client from monopolizing the loop
- periodic expiration sweeps run inside the event loop

Command execution is serialized by this thread. The AOF's once-per-second
`fsync()` helper is the only persistence background thread.

`Server::run()` owns every client session and descriptor and may run on only one
thread. `Server::stop()` is the sole cross-thread server operation: it wakes the
event loop, which closes the clients and returns. A caller running the server on
another thread must join that thread before destroying the `Server` object.

## Architecture

The server pipeline is intentionally split into small layers:

```text
TCP event loop
  -> newline command framing
  -> ArgumentSplitter
  -> Parser
  -> Command std::variant
  -> Executor
  -> StorageEngine
  -> AOF append for successful mutations
  -> ResponseFormatter
  -> buffered nonblocking write
```

`Command` is the authoritative, value-based command representation. Processing
also has one authoritative outcome: `CommandProcessResult` holds either a
processed command or an error. AOF replay decodes RESP command arguments and
feeds them back through the same command model.

### Main Components

- `Server/App/*`: command orchestration from raw command line to execution result
- `Server/Parsing/*`: argument splitting and parsing
- `Server/Commands/*`: value-based command types and command serialization
- `Server/Runtime/*`: storage engine, executor, values, and expiration handling
- `Server/Protocol/*`: response formatting and RESP AOF encoding/decoding
- `Server/Persistence/*`: append-only logging, replay, and compaction
- `Server/TCP/*`: nonblocking event loop and buffered client sessions
- `Common/Networking/*`: socket I/O shared by the server, client, and tests
- `Client/Networking/*`: TCP client implementation

## Repository Layout

```text
redisimpl/
├── Client/
│   ├── Networking/
│   ├── Tests/
│   ├── CMakeLists.txt
│   └── main.cpp
├── Server/
│   ├── App/
│   ├── Commands/
│   ├── Parsing/
│   ├── Persistence/
│   ├── Protocol/
│   ├── Runtime/
│   ├── TCP/
│   ├── Tests/
│   ├── Utility/
│   ├── CMakeLists.txt
│   └── main.cpp
├── data/
├── README.md
└── TODO.md
```

## Building

The client and server build separately.

### Server

```bash
cd Server
cmake -S . -B build
cmake --build build
```

### Client

```bash
cd Client
cmake -S . -B build
cmake --build build
```

## Running

Start the server:

```bash
./Server/build/redisserver
```

In another terminal, start the client:

```bash
./Client/build/redisclient 127.0.0.1 6380
```

### Docker

Build the server image from the repository root:

```bash
docker build -t redisimpl-server -f Server/Dockerfile Server
```

Run the container and publish the server port:

```bash
docker run --rm -p 6380:6380 redisimpl-server
```

To keep the append-only log across container restarts, mount a Docker volume at the directory the server writes to inside the container:

```bash
docker volume create redisimpl-data
docker run --rm -p 6380:6380 -v redisimpl-data:/app/data redisimpl-server
```

The server opens `data/appendonly.aof` relative to the container `WORKDIR`, so the log path inside Docker is `/app/data/appendonly.aof`.

Example session:

```text
> SET "language" "C++"
SET value="C++"

> GET "language"
GET value="C++"

> EXISTS "language"
EXISTS exists=true

> EXPIRE "language" 10
EXPIRE applied=true

> TTL "language"
TTL ttl=9

> PTTL "language"
PTTL ttl_ms=...

> PERSIST "language"
PERSIST removed=true

> DEL "language"
DEL deleted=true

> QUIT
BYE
```

## Testing

### Server tests

The server test suite covers:

- tokenization
- parsing
- command processing and result outcomes
- executor behavior
- storage engine behavior
- expiration behavior, including `TTL`, `PTTL`, and `PERSIST`
- integer arithmetic, 64-bit bounds, overflow, TTL preservation, and concurrency
- append-only logging, absolute-expiration replay, sync policies, and compaction
- malformed and truncated AOF input
- shared socket writes, including partial writes
- TCP event-loop behavior with multiple simultaneous clients and clean shutdown

Run them with:

```bash
cd Server/build
ctest --output-on-failure
```

### Client tests

The client test suite currently covers address parsing and response-buffer handling.

Run them with:

```bash
cd Client/build
ctest --output-on-failure
```

## Notes and Limitations

- The client protocol is newline-delimited text, not RESP.
- Storage and the AOF are binary-safe, but the text client protocol cannot yet
  carry every possible byte sequence end to end.
- Command execution runs on one event-loop thread and must remain nonblocking.
- Durability is append-only-log based; there is no snapshotting yet.
- Transactions, replication, pub/sub, Raft, and clustering are not implemented.
- The project is focused on learning core systems concepts, not Redis feature parity.
