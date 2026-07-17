# Audit: MachineKey

**Files**: `/Core/machinekey.h` (10 lines, header only)
**Total**: ~10 lines

## What's Good

- **Simple, correct, and well-commented.** MSB/LSB layout and purpose are documented.
- **`inline` function** avoids ODR violations.
- **No dependencies** beyond `<cstdint>`.

## What's Bad / Code Smells

### 1. No inverse function
`machineKey(x, y)` exists but no `machineKeyToXY(key)` to decode. Debugging serialized data requires ad-hoc bitwise extraction.

### 2. Not `constexpr`
Could be `constexpr` for compile-time evaluation.

### 3. Negative coordinate cast
`static_cast<uint32_t>` on negative `x` or `y` wraps to a large positive number, producing a valid-looking but wrong key.

## Could Live in the Engine

A generic `tileKey(x, y)` / `tileKeyDecode(key)` pair is useful for any 2D grid-based game. Could be part of the engine's grid utilities.

## Refactoring Suggestions

1. Make `constexpr`.
2. Add `assert(x >= 0 && y >= 0)` in debug builds.
3. Add inverse: `inline constexpr std::pair<int,int> machineKeyDecode(uint32_t key)`.

## Summary Rating

**Correct one-liner — minor hardening and a decode function would complete it.**
