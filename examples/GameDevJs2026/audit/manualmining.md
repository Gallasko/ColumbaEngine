# Audit: ManualMining

**Files**: `/Systems/manualmining.h` (137 lines), `/Systems/manualmining.cpp` (517 lines)
**Total**: ~654 lines

## What's Good

- Well-designed multi-hit mining with progress bar, decay timer, and fade-out animation.
- Tool tier gating with visual "wrong tier" red bar animation — great UX.
- Mining speed multiplier from equipped tool is properly applied.
- Ghost item float-up animation is polished (ease-out quad easing).
- Progress bar positioned in world-space above the target tile — moves with camera correctly.
- All UI entity IDs tracked for proper lifecycle management.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `LambdaCallable` adapter workaround
Lines 13-20: bridges `std::function` to the engine's `AbstractCallable`. If needed in multiple places, should be engine-provided.

### 3. Magic numbers for ghost animation
`12.0f` ghost size and `-24.0f` float distance are unnamed.

### 4. Hardcoded tick resolution
100ms chunks hardcoded rather than using a named constant.

### 5. `hotbarHeight` defaults to `48.0f`
Must match actual hotbar — fragile implicit coupling.

## Could Live in the Engine

- **World-space progress bar with fade-out tween** — very common pattern (health bars, build progress, loading indicators). Should be `WorldProgressBar` engine component.
- **`LambdaCallable` adapter** absolutely should be engine-level.

## Refactoring Suggestions

1. Move `LambdaCallable` into the engine.
2. Extract world-space progress bar into a reusable `WorldBar` class.
3. Name the ghost animation magic numbers.
4. Use a named constant for the 100ms tick resolution.

## Summary Rating

**Polished gameplay system with excellent UX details. Main concern is boilerplate for world-space UI that should be engine-provided.**
