# TODO

## High priority correctness

## Expiration correctness


## Protocol and binary safety


- [ ] Add request and value size limits to prevent unbounded buffering.

## Storage

- [ ] Avoid copying the whole value under the lock on every `get`; values can be
  up to 512 MB.

## Persistence

- [ ] Compact the AOF while the server runs. `AofCompactor` only runs at startup,
  so a long-running server's log grows without limit.
- [ ] Stream AOF compaction instead of loading the whole file: `AofCompactor` reads
  the log into one string and keeps every record, so startup needs roughly three
  times the log's size in memory.
- [ ] Lock the data directory so two servers cannot append to the same AOF.

## Command model cleanup

## Networking and lifecycle


## Commands and functionality


## Testing and quality

- [ ] Make time injectable in `StorageEngine`: only `prune_expired_batch` takes
  `now`, and the other methods call `Clock::now()` internally, so deadline edge
  cases can only be tested with past deadlines or sleeps.
- [ ] Add ThreadSanitizer, AddressSanitizer, and UndefinedBehaviorSanitizer jobs.
- [ ] Add concurrent storage, client-session lifecycle, and AOF ordering tests.
- [ ] Add crash/fault-injection tests for truncated writes, failed flushes,
  interrupted compaction, and restart recovery.

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
