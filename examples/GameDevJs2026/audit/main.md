# Audit: main.cpp

**Files**: `/main.cpp` (13 lines)
**Total**: ~13 lines

## What's Good

- **Minimal and correct.** Just creates the app and runs it.
- **`static` for Emscripten compatibility** is well-commented and necessary for the WASM target.
- **`sync_with_stdio(false)`** is a minor but welcome performance hint.

## What's Bad / Code Smells

### 1. `argc`/`argv` are unused
Should be marked `[[maybe_unused]]` or cast to `(void)` to silence compiler warnings on strict builds.

### 2. No top-level exception handler
If `GameApp` or `exec()` throws, the program terminates with no diagnostic. A `try/catch(std::exception& e)` with an error log would improve debugging.

## Could Live in the Engine

The `static GameApp` + Emscripten pattern could be encapsulated in an engine-level `PG_MAIN(AppClass)` macro that handles the `static` trick and `sync_with_stdio` automatically.

## Summary Rating

**Textbook entry point — clean, no issues beyond minor polish.**
