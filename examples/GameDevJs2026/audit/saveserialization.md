# Audit: SaveSerialization

**Files**: `/Core/saveserialization.h` (74 lines), `/Core/saveserialization.cpp` (257 lines)
**Total**: ~331 lines

## What's Good

- **Centralized serialization.** All game-specific types have their serialize/deserialize specializations in one place — easy to audit what gets saved.
- **Null checks** (`if (s.isNull()) return result;`) on every deserialize — good defensive coding.
- **Consistent pattern.** Every type follows `startSerialization` / fields / `endSerialization` for serialization, and field-by-field `defaultDeserialize` for deserialization. Easy to add new types by copy-paste.
- **Forward declarations** in the header avoid pulling in heavy system headers.

## What's Bad / Code Smells

### 1. `using namespace pg;` in the header — MOST HARMFUL INSTANCE
`saveserialization.h` is included by multiple .cpp files. Every includer silently gets the entire `pg` namespace injected. **Must be removed.**

### 2. Type narrowing without validation
```cpp
unsigned int tileId = 0;
defaultDeserialize(s, "tileId", tileId);
result.tileId = static_cast<uint16_t>(tileId);
```
If the saved value exceeds 65535, it silently truncates. Same for all `uint8_t` fields.

### 3. No versioning
No save format version number. If a field is added, renamed, or removed, old save files produce wrong data silently. A version tag at the top of each serialized type would enable migration.

### 4. No error logging on deserialization failure
If `defaultDeserialize` fails to find a field, the value stays at default with no warning. A corrupted save silently produces broken game state.

### 5. Repetitive boilerplate
`StorageData` and `DepotData` serialize/deserialize pairs are nearly identical. A macro or template helper could generate these.

## Could Live in the Engine

- **A serialization macro/helper** — the repetitive `serialize(archive, "fieldName", value.field)` pattern is a prime candidate for `PG_SERIALIZE_FIELD(archive, value, field)`.
- **Version-tagged serialization** — the engine's archive framework could support automatic version tags.

## Refactoring Suggestions

1. **Remove `using namespace pg;` from the header** — qualify types explicitly.
2. **Add save format versioning.**
3. **Add range-check assertions** on narrowing casts during deserialization.
4. **Log warnings** when `defaultDeserialize` encounters missing fields.
5. **Consider a serialization macro** to reduce boilerplate.

## Summary Rating

**Functional and consistent, but `using namespace pg;` in the header is actively harmful, and the lack of versioning is a ticking time bomb for save compatibility.**
