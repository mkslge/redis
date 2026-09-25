# TODO

## High priority correctness

## Expiration correctness

- [ ] Bound `StorageEngine`'s expiration queue. Every `EXPIRE` enqueues a candidate
  and stale candidates are dropped only when the sweep reaches them (at most about
  2,000 per second), so repeated `EXPIRE` on one key grows memory without limit.
  Rebuild the queue when it exceeds twice the number of keys with deadlines.

## Protocol and binary safety


- [ ] Make response formatting length-aware so arbitrary bytes can be returned.
- [ ] Add request and value size limits to prevent unbounded buffering.

## Storage

- [ ] Replace the hand-maintained expiration tracking in `StorageEngine::set`, `del`,
  `persist`, `expire_at`, and `restore_state` with private `track_deadline` and
  `untrack_deadline` helpers.
- [ ] Rename `StorageEngine::possibly_expired` to `keys_with_deadlines` (it returns
  keys that have a deadline), and document that `size()` deletes every expired key
  in a full scan under the lock.
- [ ] Avoid copying the whole value under the lock on every `get`; values can be
  up to 512 MB.

## Command model cleanup

## Networking and lifecycle

- [ ] Fix the server Docker build. `docker build -f Server/Dockerfile Server` sends
  only `Server/` as the build context, but `Server/CMakeLists.txt` also needs
  `../Common`, so the image fails to build. Build from the repository root instead.
- [ ] Add connection, read, and write timeouts suitable for deployment.

## Commands and functionality

- [ ] Optimize AOF compaction so repeated mutations such as setting the same key
  many times retain only the state needed for recovery.

## Testing and quality

- [ ] Fix flaky shutdown tests: `ShutdownClosesClientsInDifferentSessionStates`
  fails about half the time on macOS (a connection sometimes never sees the close),
  and `ConcurrentStopRequestsAreIdempotent` fails about 1 run in 20.
- [ ] Add direct `StorageEngine::adjust_integer` tests (overflow, `INT64_MIN`,
  missing key, deadline preservation); they are only covered through `testExecutor`.
- [ ] Make time injectable in `StorageEngine`: only `prune_expired_batch` takes
  `now`, and the other methods call `Clock::now()` internally, so deadline edge
  cases can only be tested with past deadlines or sleeps.
- [ ] Add ThreadSanitizer, AddressSanitizer, and UndefinedBehaviorSanitizer jobs.
- [ ] Add concurrent storage, client-session lifecycle, and AOF ordering tests.
- [ ] Add crash/fault-injection tests for truncated writes, failed flushes,
  interrupted compaction, and restart recovery.
- [ ] Add end-to-end binary tests containing null bytes, CRLF, quotes, and `0xFF`.

## Distributed roadmap

- [ ] Separate command parsing/proposal from state-machine application.
- [ ] Introduce a durable Raft WAL with term, vote, indexed entries, checksums,
  commit metadata, and recovery.
- [ ] Implement deterministic Raft leader election and log replication for a
  fixed three-node cluster before attempting dynamic membership or sharding.
- [ ] Add snapshots, follower catch-up, leader redirection, linearizable reads,
  metrics, and reproducible failure demonstrations.

## Out of Scope For Now

- [ ] Implement the required RESP2 subset: arrays, bulk strings, simple strings,
  errors, integers, and null bulk strings.
