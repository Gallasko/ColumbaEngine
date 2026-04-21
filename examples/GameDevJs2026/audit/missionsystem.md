# Audit: MissionSystem

**Files**: `/Systems/missionsystem.h` (86 lines), `/Systems/missionsystem.cpp` (250 lines)
**Total**: ~336 lines

## What's Good

- Well-structured tier-gated mission system with unlock chaining via WorldFacts.
- `claimMission` gracefully handles removed depots (rewards go directly to player).
- Rewards deposited into depot with overflow to player — good edge case handling.
- Per-mission serialization with prefix-based keys is straightforward.
- `purchaseExtraSlot()` is a clean progression mechanism.
- Bounds check on `defIndex` in `isMissionUnlocked` and `load`.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `ROBOT_CORE_ID = 33` — hardcoded item ID
Named constant is better than inline `33`, but should come from `ItemRegistry`, not a hardcoded number.

### 3. `buildMissionDefs` is entirely hardcoded
All reward item IDs are magic numbers (`{35, 3}`, `{1, 5}`, etc.). Should be loaded from config or use named item constants.

### 4. `getExtraSlotCost()` returns hardcoded `10`
Should be configurable.

### 5. No duplicate-mission check
Same mission type can be started multiple times simultaneously.

## Could Live in the Engine

A generic "timed task with requirements and rewards" system, gated by a fact/prerequisite graph, is useful for quests/research in many games.

## Refactoring Suggestions

1. Load mission definitions from a data file.
2. Reference item IDs by name through the registry.
3. Add a flag for whether a mission can run concurrently.

## Summary Rating

**Functional progression system, but hardcoded item IDs in mission definitions are a data/code separation concern.**
