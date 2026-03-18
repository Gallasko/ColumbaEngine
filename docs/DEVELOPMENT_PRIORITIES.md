# Development Priorities

This document tracks the engine's current performance baseline, honest competitive position, and prioritised work items. It is updated as benchmarks are re-run and priorities shift.

---

## Benchmark Baseline (100k entities, Release build)

Run with `./bench --gtest_filter="ECS_Benchmark.Summary"`.

| Operation | Total | Per entity |
|---|---|---|
| Entity creation | 5,469 μs | 54.7 ns |
| Create + attach (1 component) | 41,892 μs | 418.9 ns |
| Iteration read (1 component) | 393 μs | 3.93 ns |
| Apply velocity (2 components) | 1,218 μs | 12.18 ns |

### What these numbers mean

**Iteration (3.93 ns/entity) is the headline.** This places the engine in the same performance tier as EnTT and Flecs — the two most respected C++ ECS libraries — and 10–50× ahead of Godot's node-based iteration. This is the number to lead with in any public benchmark or comparison.

**Component attachment (418.9 ns/entity) is the primary technical debt.** This is roughly 8× slower than EnTT for the equivalent operation. Since attachment happens at load/spawn time rather than every frame it does not affect frame performance directly, but it does affect level load times and large-scale spawning. This needs to be investigated and improved before the engine can be recommended for projects that spawn large numbers of entities at runtime.

**Two-component iteration (12.18 ns/entity) is fixable.** The benchmark currently uses parallel `view<>` iterators in lockstep, which is not cache-coherent. The group-based approach (`registerGroup<A, B>()`) pre-joins the two component sets and should bring this down to approximately 5–6 ns/entity, closing the gap with single-component iteration.

---

## Competitive Position

| Engine | Single-comp iteration | 2-comp iteration | Notes |
|---|---|---|---|
| **PgEngine** | **~4 ns** | **~12 ns (view) / ~5 ns est. (group)** | Pure ECS, C++17 |
| EnTT | ~1–3 ns | ~2–4 ns | Groups fully implemented |
| Flecs | ~1–3 ns | ~2–3 ns | Archetype-based |
| Unity DOTS + Burst | ~2–5 ns | ~3–6 ns | C# with AOT compiler |
| Godot (Nodes) | ~50–200 ns | — | Node overhead |
| Unity GameObject | ~100–500 ns | — | OOP overhead |

**Honest assessment:** The iteration story is strong and defensible. The creation/attachment story is not yet competitive. The engine should not be positioned as a high-performance spawning system until the attachment cost is addressed.

---

## Priority 1 — Fix the Component Attachment Bottleneck

**Why first:** It is the largest gap versus comparable ECS frameworks and the most likely thing a technically literate user will notice when they run their own benchmarks.

**Investigation steps:**

1. Profile `attachGeneric` in isolation (separate from `createEntity`) to confirm the cost is in attachment, not entity allocation.
2. Check whether the `registry.hasTypeId<Type>()` check and the associated log warning path are contributing — this runs on every `attachGeneric` call and hits a hash map.
3. Examine the entity component list update (`entity->componentList`) — if this is a `std::vector`, repeated push-backs at large scale will cause reallocations.
4. Compare `attachGeneric` performance before and after calling `fakeStart()` — the code branches between `cmdDispatcher.attachComp` (running) and `registry.retrieve<Type>()->internalCreateComponent` (not running), and one path may be significantly slower.

**Target:** Bring create + attach (1 component) below 150 ns/entity, closing the gap with EnTT.

---

## Priority 2 — Group-Based Multi-Component Benchmark

**Why second:** The group system already exists and is the correct ECS pattern for multi-component iteration. Adding a group-based benchmark will:

- Likely halve the 2-component iteration time (12 ns → ~5–6 ns)
- Give a more accurate picture of what the engine actually achieves in real system code (systems use groups, not raw views)
- Produce a better marketing number for the "apply velocity" benchmark

**Implementation:** Add a `TEST(ECS_Benchmark, GroupApplyVelocity)` in [benchmark/ecs_performance.cc](../benchmark/ecs_performance.cc) using `registerGroup<BenchPosition, BenchVelocity>()` inside a system's `execute()`. This requires a system to be registered and the ECS to be running (use `fakeStart()` + `executeOnce()`).

**Target:** 2-component group iteration below 6 ns/entity.

---

## Priority 3 — Publish the Benchmark

Once Priorities 1 and 2 are done, publish a comparison page. The format that resonates with technical audiences is a simple table and a graph, showing entity counts on the X axis and frame time on the Y axis.

**Comparison targets to include:**
- EnTT (the C++ ECS standard, easy to benchmark independently)
- Godot 4 (Node-based, as a reference point for "traditional" engine overhead)
- Unity DOTS (as a reference point for the mainstream ECS offering)

**The narrative:** "Pure ECS designed from the ground up, not bolted on. Iteration performance on par with the best standalone ECS libraries, inside a complete 2D game engine."

---

## Priority 4 — Feature Gaps Versus Competitors

These are the features that prevent users from choosing this engine for projects they could otherwise build with it. Ordered by how often they come up in game jam and indie project contexts.

### P4.1 — Mature Networking (Medium effort, high value)

The networking system works but is not battle-tested. Multiplayer is a common game jam requirement. Even a reliable "two players on the same machine over localhost" demo would significantly expand the engine's appeal.

**Minimum viable goal:** A working two-player example (not just the raw client-server demo) that demonstrates synchronized entity state across two clients.

### P4.2 — 3D Rendering (High effort, high value)

The infrastructure exists (3D camera, mesh loading, OpenGL pipeline, TinyGLTF). The gap is the rendering system itself. A minimal 3D renderer (textured meshes, one directional light, no shadows) would open the engine to a much larger audience.

**Minimum viable goal:** Render a GLTF model with a basic Phong or PBR shader.

### P4.3 — Polished Editor (Medium effort, medium value)

The editor exists but is not a selling point. Godot's editor is a genuine competitive advantage for that engine. The editor does not need to match Godot — it needs to be "good enough that a developer does not want to hand-code all scene setup."

**Minimum viable goal:** Entity inspector, component property editing, and scene save/load from inside the editor without touching code.

### P4.4 — Community Documentation (Low effort, high leverage)

The API surface is large (~174 headers) and documentation coverage is sparse. A developer who cannot figure out how to do something in 10 minutes will leave. Even one well-written guide per major subsystem (ECS, rendering, audio, scripting) would significantly lower the barrier to entry.

**Minimum viable goal:** One tutorial that takes a user from `EmptyAppTemplate` to a moving sprite with collision detection, written in the style of the Godot "Your first 2D game" guide.

---

## Priority 5 — Game Jam Presence

Game jams are where engines grow. You have existing jam entries (PixelJam, GameOff, GMTK2025). The next step is to lower the barrier for *other people* to use the engine in jams.

**Actions:**
- Post the engine to itch.io with a clear one-paragraph pitch and a link to the EmptyAppTemplate
- Write a "make a game in 48 hours with PgEngine" quick-start guide
- Document the WebAssembly export workflow (this is a differentiator — jam games need browser play)

---

## What Not to Prioritise Now

- **Mobile (iOS/Android):** Too much infrastructure work for the current stage.
- **Visual scripting:** PgScript already covers the scripting use case; visual scripting duplicates it.
- **Asset store:** Community size doesn't support this yet; focus on building the community first.
- **Matching Unity's feature set:** Impossible and not the goal. Own the "pure ECS 2D engine you can understand completely" niche instead of competing on breadth.
