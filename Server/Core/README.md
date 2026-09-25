# Core

Vocabulary types and small helpers shared by every server module. Header-only.

- **Depends on:** nothing but the standard library.
- **Used by:** every other server module.

## Files

| File | Provides |
|---|---|
| `Bytes.h` | `Bytes`: a binary-safe byte string (`std::string`). |
| `CommandArguments.h` | `CommandArguments`: one command as byte strings, name first. |
| `Key.h` | `Key`: an alias of `Bytes`. |
| `Value.h` | `Value`: a stored value wrapping `Bytes`. |
| `Integer.h` | `parse_integer`: the strict signed 64-bit decimal parser. |
| `Overloaded.h` | `Overloaded` and `OneOf`: helpers for `std::visit`. |
| `CMakeLists.txt` | `SERVER_CORE`, which also sets the include root for the whole server. |

## Rules

- **`Bytes` is not text.** It may contain `\0`, `\r\n`, and any other byte. Never
  use `c_str()` or C-string functions on it.
- **`Key` is only an alias**, so the compiler cannot catch a key passed where a value
  belongs. `Value` is a distinct type, and both of its constructors are `explicit`.
- **Parse numbers from client input only with `parse_integer`.** It follows Redis's
  rules: no `+`, leading zeros, `-0`, or decimals. Do not add a second parser.
- **Visit `Command` with `Overloaded`**, using `OneOf<...>` to handle a group of
  alternatives in one lambda. A new command that isn't handled must fail to compile,
  so a generic `auto` lambda may only forward to something every command provides
  (as `is_mutating` and `Executor::execute` do), never supply default behavior.
- **Include by module path**, for example `#include "Core/Bytes.h"`. Linking
  `SERVER_CORE`, directly or through any other module, provides that include root.
- **What belongs here:** only types or helpers that at least two modules need and that
  depend on nothing else. Everything else lives in the module that owns it.

## Tests

Core has no test file of its own. `parse_integer` is covered through `testParser`
(`EXPIRE`) and `testExecutor` (`INCRBY`), and the other headers are used by every
suite.
