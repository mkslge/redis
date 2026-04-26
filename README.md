# My implementation of redis



The project currently includes:

- a TCP server with a simple newline-delimited text protocol
- a CLI client for interactive testing
- typed values (`int`, `double`, `char`, `string`)
- key expiration with both lazy pruning and a background expiration thread
- append-only logging for durable mutation replay
- unit and integration tests for the parser, runtime, persistence, networking, and client

## Current Status

This is a working educational systems project, not a Redis-compatible clone.

Implemented today:

- `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`, `QUIT`, `EXIT`
- append-only log writes for mutating commands
- log replay on server startup
- one worker thread per connected client
- a separate expiration manager thread
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
- `QUIT`
- `EXIT`

### Value Types

The parser currently accepts these scalar value types:

- `int`
- `double`
- `char`
- `string`

Examples:

```text
SET "name" "mark"
SET "age" 22
SET "pi" 3.14159
SET "middle_initial" 'A'
GET "name"
EXISTS "age"
EXPIRE "name" 30
DEL "pi"
QUIT
```

Keys are normalized to strings internally, even if the input token is numeric or character-based.

## Wire Protocol

The server uses a simple newline-delimited text protocol rather than RESP.

Example responses:

```text
SET value="C++"
GET value="C++"
EXISTS exists=true
EXPIRE applied=true
DELETE deleted=true
BYE
ERROR parse failure
```

## Persistence

Mutating commands are appended to:

`Server/Persistence/appendonlylog.txt`

On startup, the server replays that file before accepting client traffic.

## Concurrency Model

The server is no longer single-client.

- `Server::run()` accepts new TCP connections on the main server thread
- each accepted client gets its own worker thread
- finished client threads are reaped and joined by the server loop
- expiration is handled by a separate background thread

This keeps the networking model simple while still allowing multiple simultaneous clients.

## Architecture

The server pipeline is intentionally split into small layers:

```text
TCP input
  -> Tokenizer
  -> Parser
  -> Statement AST
  -> Executor
  -> StorageEngine
  -> Response formatting
  -> Optional AOF append
```

### Main Components

- `Server/App/*`: command orchestration from raw command line to execution result
- `Server/Parsing/*`: tokenization and parsing
- `Server/Commands/*`: command statement types
- `Server/Runtime/*`: storage engine, executor, values, and expiration handling
- `Server/Protocol/*`: response formatting
- `Server/Persistence/*`: append-only logging and replay
- `Server/TCP/*`: socket server, connection lifecycle, and per-client request handling
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
docker run --rm -p 6380:6380 -v redisimpl-data:/app/Persistence redisimpl-server
```

The server opens `Persistence/appendonlylog.txt` relative to the container `WORKDIR`, so the log path inside Docker is `/app/Persistence/appendonlylog.txt`.

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

> DEL "language"
DELETE deleted=true

> QUIT
BYE
```

## Testing

### Server tests

The server test suite covers:

- tokenization
- parsing
- executor behavior
- storage engine behavior
- expiration manager behavior
- append-only logging and replay
- TCP integration behavior

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

- The protocol is intentionally simple and human-readable.
- The project is focused on learning core systems concepts, not Redis feature parity.
- Durability is append-only-log based; there is no snapshotting yet.
- Current value support is limited to scalar primitives and strings.
