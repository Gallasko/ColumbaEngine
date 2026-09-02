# SimpleBoxBouncer

**Build target:** `BoxBouncer` · **Start here if you're new.**

The minimal ColumbaEngine example: a `pg::Engine` app with a single custom system (`BoxBouncerSystem`) that spawns one square via `makeSimple2DShape`, moves it every `TickEvent`, flips velocity and randomizes color on wall hits, and persists a bounce count across runs with the `SaveSys` trait.

What it demonstrates:
- `pg::Engine` + `setSetupFunction` — zero platform boilerplate
- Defining a system with `System<InitSys, Listener<TickEvent>>`
- Creating an entity and attaching a custom component (`attachGeneric`)
- The accumulate-in-`onEvent` / mutate-in-`execute()` pattern
- Simple save/load with `SaveSys`

Run from the build directory: `./BoxBouncer`

Next step: the Breakout tutorial in `docs/source/tutorials.rst` grows this exact structure into a full game.
