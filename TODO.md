# TODO

## High priority correctness

## Expiration correctness


## Protocol and binary safety

- [ ] Implement the required RESP2 subset: arrays, bulk strings, simple strings,
  errors, integers, and null bulk strings.
- [ ] Decode command arguments as byte strings. Only commands that require
  numbers, such as `EXPIRE` and `INCR`, should parse their arguments numerically.
- [ ] Remove the token variant/source-text dual representation after RESP parsing
  becomes the server command boundary.
- [ ] Make response formatting length-aware so arbitrary bytes can be returned.
- [ ] Add request and value size limits to prevent unbounded buffering.

## Command model cleanup

- [x] Replace the duplicated `StatementType` plus dynamic-class representation
  with one authoritative command representation, likely `std::variant`. This
  should also become the serializable command format used by the future Raft log.
- [x] Replace unchecked `dynamic_cast` dereferences in `Executor`.
- [ ] Replace `CommandProcessResult`'s two independent optionals with a type that
  represents exactly one outcome, such as `std::variant<ProcessedCommand, Error>`.
- [ ] Remove repetitive parser pass-through functions and accept token vectors by
  const reference where parsing does not modify them.
- [ ] Remove repeated statement key/serialization boilerplate where the new
  command representation makes it unnecessary.

## Networking and lifecycle

- [ ] Include `errno` details in socket errors so bind, connect, accept, send, and
  receive failures are diagnosable.
- [ ] Simplify server worker-thread bookkeeping and verify descriptor reuse,
  shutdown, and worker completion cannot race.
- [ ] Add connection, read, and write timeouts suitable for deployment.

## Commands and functionality

- [ ] Add `APPEND`.
- [ ] Add `INCR`, `DECR`, `INCRBY`, and `DECRBY`. Numeric commands should parse
  byte-string values on demand and store their results as bytes.
- [ ] Optimize AOF compaction so repeated mutations such as setting the same key
  many times retain only the state needed for recovery.

## Testing and quality

- [ ] Add ThreadSanitizer, AddressSanitizer, and UndefinedBehaviorSanitizer jobs.
- [ ] Add concurrent storage, client-session lifecycle, and AOF ordering tests.
- [ ] Add crash/fault-injection tests for truncated writes, failed flushes,
  interrupted compaction, and restart recovery.
- [ ] Add end-to-end binary tests containing null bytes, CRLF, quotes, and `0xFF`.
- [ ] Remove dead code such as unused lowercase/token-name helpers when confirmed
  unnecessary.

## Distributed roadmap

- [ ] Separate command parsing/proposal from state-machine application.
- [ ] Introduce a durable Raft WAL with term, vote, indexed entries, checksums,
  commit metadata, and recovery.
- [ ] Implement deterministic Raft leader election and log replication for a
  fixed three-node cluster before attempting dynamic membership or sharding.
- [ ] Add snapshots, follower catch-up, leader redirection, linearizable reads,
  metrics, and reproducible failure demonstrations.
