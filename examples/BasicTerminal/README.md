# BasicTerminal

**Build target:** `BasicTerminal`

A line-numbered text editor with Open/Save dialogs, built as a *raw SDL* app (it manages its own `pg::Window`, init thread, and main loop instead of using `pg::Engine`). Useful both as a text-handling reference and as a look at what `pg::Engine` abstracts away.

What it demonstrates:
- Consuming `OnSDLTextInput`, `OnSDLScanCode`, and `OnSDLScanCodeReleased` with `QueuedListener`
- Building per-line UI out of `Prefab` helpers and `TTFText` (`setText` for live updates)
- Clickable text buttons via `MouseLeftClickComponent` + `makeCallable`
- Native file dialogs through the bundled `tinyfiledialogs`

Run from the build directory: `./BasicTerminal`
