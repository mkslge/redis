# redisimpl

A Redis-inspired database written in modern C++.

This project is a from-scratch implementation of a small in-memory key-value store with a custom command pipeline, TCP server/client binaries, typed values, TTL support, and append-only logging for durability experiments.




### Server and Client

- TCP server listening on port `6380` by default
- CLI client for sending commands over the wire
- newline-delimited text protocol
- One-client-at-a-time request loop per accepted connection

### Supported Commands

- `SET <key> <value>`
- `GET <key>`
- `DEL <key>`
- `EXISTS <key>`
- `EXPIRE <key> <seconds>`
- `QUIT` / `EXIT`

### Data Model

Supported value types:

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
```

### Persistence

Mutating commands are appended to `Server/Log/appendonlylog.txt`.



## Architecture

The codebase is intentionally split into focused layers.

```text
Client input / TCP message
        |
        v
   Tokenizer
        |
        v
     Parser
        |
        v
    Statement AST
        |
        v
    Executor
        |
        v
  StorageEngine
        |
        v
 Response + optional AOF log
```

### Main Components

- `Server/TCP/Server.*`: socket setup, connection loop, request framing, and response sending
- `Server/Interpreter/Parsing/*`: tokenization and parsing into typed statements
- `Server/Interpreter/Statements/*`: command representations (`GET`, `SET`, `DEL`, `EXISTS`, `EXPIRE`)
- `Server/Interpreter/Runtime/*`: execution layer and storage engine
- `Server/Log/AOFLogger.*`: append-only file logging for mutating operations
- `Client/Networking/Client.*`: TCP client implementation

## Repository Layout

```text
redisimpl/
├── Client/
│   ├── Networking/
│   ├── Tests/
│   └── main.cpp
├── Server/
│   ├── Interpreter/
│   │   ├── Model/
│   │   ├── Parsing/
│   │   ├── Runtime/
│   │   ├── Statements/
│   │   └── Tokens/
│   ├── Log/
│   ├── TCP/
│   ├── Tests/
│   └── main.cpp
└── TODO.md
```

## Building the Project
This repository currently builds the server and client as separate CMake projects.


### Build the Server

```bash
cd Server
cmake -S . -B build
cmake --build build
```

### Build the Client

```bash
cd Client
cmake -S . -B build
cmake --build build
```

## Running It

Start the server:

```bash
./Server/build/redisserver
```

In another terminal, connect with the client:

```bash
./Client/build/redisclient 127.0.0.1 6380
```

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
```

## Testing
The project includes unit tests for both the server and client components.
You can run them with `ctest --build-dir build`

### Server tests

- tokenizer behavior
- parser behavior
- executor behavior
- storage engine behavior

Run them with:

```bash
cd Server/build
ctest --output-on-failure
```

### Client tests

Run them with:

```bash
cd Client/build
ctest --output-on-failure
```

## Implementation Notes

A few implementation details are worth calling out:

- Keys are normalized into strings internally, even when the input token is numeric or character-based.
- TTL expiration is enforced lazily during reads and key checks.
- The wire protocol is intentionally simple so the parsing and execution pipeline stays easy to reason about.
- Responses are structured plain text rather than Redis RESP. May switch to RESP later

