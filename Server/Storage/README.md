# Storage

The in-memory keyspace: one `StorageEngine` mapping each `Key` to a `Value` and an
optional expiration deadline, guarded by one mutex.

- **Depends on:** [Core](../Core/README.md) (`Key`, `Value`, `parse_integer`).
- **Used by:** `Commands/Executor` (every command) and `Network/Server` (the
  periodic expiration sweep).

## Files

| File | Provides |
|---|---|
| `StorageEngine.h` / `.cpp` | `StorageEngine`, its operations, and their result types. |
| `CMakeLists.txt` | `SERVER_STORAGE`. |

## How expiration works

A key past its deadline is removed in two ways:

1. **On access.** Every operation that reads or updates an existing key first
   deletes it if the deadline has passed, so no caller ever sees an expired value.
   (`set` and `restore_state` replace the whole entry, so they skip this.)
2. **By the sweep.** `Network/Server` calls `prune_expired_batch` every 100 ms with a
   budget of 200 candidates. Candidates come from a queue of `(key, generation)`
   pairs. Each time a key's deadline is set, it gets a new generation and is
   enqueued; a candidate whose generation is no longer current is skipped, and one
   that has not expired yet is re-enqueued.

## Rules

- **Each public method is atomic** under the single mutex. Nothing is atomic across
  calls, so a check-then-act sequence must be one method.
- **Deadlines are absolute `system_clock` (wall-clock) times**, the same values the
  AOF stores. A deadline in the past deletes the key immediately, in both
  `expire_at` and `restore_state`.
- **Keep expiration tracking in sync.** A method that removes a deadline must erase
  the key from `expiration_generations_`; one that sets a deadline must assign a new
  generation and enqueue it. Five methods currently do this by hand (see TODO).
- **`set` clears any deadline.** Integer operations keep it.
- **Integer operations** require canonical decimals (`parse_integer`) and treat a
  missing key as 0. They report `IntegerError` values; the message text belongs to
  `Commands/Errors.h`.
- **Storage knows nothing about commands or the AOF.** It returns resulting values
  and deadlines, and upper layers decide what to log or respond.
- **`ttl_milliseconds` uses Redis sentinels:** -2 for a missing key, -1 for no
  deadline. Rounding to seconds for `TTL` happens in the Executor.
- **The "Diagnostics and test support" methods are for tests only.** Note that
  `possibly_expired()` returns every key with a deadline, and `size()` deletes all
  expired keys in a full scan under the lock.

## Adding an operation

Take the lock, call `prune_if_expired_unlocked` for the key, keep expiration
tracking in sync if the deadline changes, and return data (a value, a flag, or an
enum) rather than error text.

## Tests

`testStorageEngine`. Integer operations are covered only through `testExecutor`.
Most methods read `Clock::now()` internally, so deadline tests use deadlines in the
past rather than sleeping.
