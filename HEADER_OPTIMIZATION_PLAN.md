# EntitySystem.h Compile Time Optimization Plan

## Priority 1: High Impact, Low Risk (Do These First)

### 1. Remove Taskflow include (~40% compile time reduction)
**File**: `src/Engine/ECS/entitysystem.h`

```cpp
// REMOVE line 7:
// #include "taskflow/taskflow.hpp"

// ADD after line 6:
namespace tf {
    class Taskflow;
    class Executor;
    class Task;
}
```

**File**: `src/Engine/ECS/entitysystem.cpp`
```cpp
// ADD at top:
#include "taskflow/taskflow.hpp"
```

**Impact**: Massive - taskflow.hpp is 10,000+ lines of templates
**Risk**: Low - only used as private members

---

### 2. Forward-declare SaveManager (~5% reduction)
```cpp
// REMOVE line 13:
// #include "savemanager.h"

// ADD forward declaration:
class SaveManager;
```

Move include to entitysystem.cpp

**Impact**: Medium
**Risk**: Low - only used as private member (line 1077)

---

### 3. Forward-declare CommandDispatcher (~5% reduction)
```cpp
// REMOVE line 12:
// #include "commanddispatcher.h"

// ADD forward declaration:
class CommandDispatcher;
```

Move include to entitysystem.cpp

**Impact**: Medium
**Risk**: Low - only used as private member (line 1073)

---

## Priority 2: Medium Impact, Medium Risk

### 4. Extract inline EventDispatcher class (~3% reduction)

Create new file: `src/Engine/ECS/internal/eventdispatcher.h`

Move lines 86-108 from entitysystem.h to this file.
Include it only in entitysystem.cpp

**Impact**: Small-Medium
**Risk**: Medium - need to ensure proper include ordering

---

### 5. Reduce serialization.h dependency (~5% reduction)

```cpp
// REMOVE line 15:
// #include "serialization.h"

// ADD at end of file (before template implementations):
#include "serialization.h"  // Only needed for templates
```

Or create serialization_fwd.h with just forward declarations.

**Impact**: Medium
**Risk**: Medium - used in templates

---

## Priority 3: Structural Changes (Do Later)

### 6. Create entitysystem.inl for template implementations

Extract all template implementations (lines 1101-1588) to:
`src/Engine/ECS/entitysystem.inl`

Include at bottom of entitysystem.h:
```cpp
#include "entitysystem.inl"
```

**Impact**: Small (just organization)
**Risk**: Low
**Benefit**: Makes header more readable

---

### 7. Reduce Profile include dependencies (~2% reduction)

```cpp
#ifdef PROFILE
// REMOVE lines 21-29 includes
// ADD forward declarations:
namespace std {
    class mutex;
    template<typename K, typename V> class unordered_map;
}
#endif
```

Move actual includes to entitysystem.cpp

---

## Priority 4: Advanced Optimizations

### 8. Create two-tier header system

**Create**: `entitysystem_fwd.h` (already created)
- Just forward declarations
- Use this in files that only need pointers/references

**Update**: Other headers to use `entitysystem_fwd.h` instead of full header

**Impact**: Large (for downstream files)
**Risk**: Low
**Effort**: High - need to update many files

---

## Measurement

Before starting, measure current compile time:
```bash
time make clean && time make -j$(nproc) ColumbaEngine
```

After each change, measure again to verify improvement.

Target: 30-50% reduction in compile time for entitysystem.h and dependent files.

---

## Notes

- entity.h and system.h CANNOT be forward-declared because templates instantiate their methods
- Keep logger.h because LOG_THIS_MEMBER is used in inline methods
- STL headers (<vector>, <unordered_map>, <algorithm>) must stay for template code
- componentregistry.h might be reducible but needs careful analysis of template usage
