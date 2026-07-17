# Audit: AutosaveSystem

**Files**: `/Systems/autosavesystem.h` (33 lines, header only)
**Total**: ~33 lines

## What's Good

- Clean, minimal, header-only — appropriate for a system this simple.
- `DEFAULT_INTERVAL_MS` is a named `constexpr`, not a magic number.
- Uses subtraction-based accumulator (`-=`) to avoid drift — correct for timed ticks.

## What's Bad / Code Smells

### 1. Double-save on freeze
`accumulator -= intervalMs` means if the game freezes for 6+ minutes, two saves fire in quick succession. Using `accumulator = 0` would be safer.

### 2. Tick truncation
`static_cast<size_t>(event.tick)` truncates fractional ms every frame — accumulates drift over time. This pattern appears in nearly every tick-driven system (project-wide issue).

## Could Live in the Engine

An `IntervalSystem<EventType>` or `TimedCallback` helper would eliminate this boilerplate. Autosave is a common need.

## Summary Rating

**Clean and simple — no bugs, just a minor truncation concern.**
