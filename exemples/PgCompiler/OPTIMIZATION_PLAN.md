# Loop Optimization Pass Pipeline

## Target Code Pattern
```cpp
var a = 0;
for (var c = 0; c < 10000; c++) {
    for (var b = 0; b < 100; b++) {
        while (a < 10) {
            a++;
        }
    }
}
```

**Goal:** Optimize to `a = 10`

## Complete Pass Pipeline

### Phase 1: Basic Cleanup
1. **ConstantUniformityPass** ✓ (exists)
   - Removes duplicate constants
2. **ConstantPropagationPass** (IMPLEMENTING NOW)
   - Replaces `var = constant; load var` with direct constants

### Phase 2: Loop Analysis & Invariant Detection
3. **LoopNestAnalysisPass** (NEW)
   - Identifies nested loop structures and variable access patterns
4. **LoopInvariantAnalysisPass** (NEW)
   - Detects that `a` reaches 10 in first inner loop iteration
   - Marks invariant conditions across loop nests

### Phase 3: Loop Transformations
5. **InnerLoopSimplificationPass** (NEW - extends current pass)
   - Converts `while(a < 10) a++` to `a = 10` when a starts < 10
   - Handles global variable increment loops
6. **LoopInvariantMotionPass** (NEW)
   - Moves `a = 10` assignment outside nested loops
7. **DeadLoopEliminationPass** (NEW)
   - Eliminates loops with no effect after first iteration

### Phase 4: Control Flow Simplification
8. **ControlFlowSimplificationPass** (NEW)
   - Removes empty loop bodies and unreachable jumps
9. **DeadCodeEliminationPass** (NEW)
   - Removes unused variables (b, c) and pop operations

### Phase 5: Final Cleanup
10. **LongJumpOptimizationPass** ✓ (exists)
11. **PeepholeOptimizationPass** (NEW)

## Execution Order
```cpp
// Phase 1: Basic cleanup
ConstantUniformityPass
ConstantPropagationPass

// Phase 2: Analysis passes
LoopNestAnalysisPass
LoopInvariantAnalysisPass

// Phase 3: Loop transformations (multiple iterations)
for (int i = 0; i < 3; ++i) {
    InnerLoopSimplificationPass
    LoopInvariantMotionPass
    DeadLoopEliminationPass
}

// Phase 4: Control flow cleanup
ControlFlowSimplificationPass
DeadCodeEliminationPass

// Phase 5: Final optimization
LongJumpOptimizationPass
PeepholeOptimizationPass
```

## Key Challenges
- Cross-loop dependency tracking
- Global variable analysis (a persists across loop boundaries)
- Three levels of nesting requiring sophisticated analysis

## Implementation Priority
1. **ConstantPropagationPass** - Start here (immediate need)
2. **InnerLoopSimplificationPass** - Extend current pass for global variables
3. **LoopInvariantAnalysisPass** - Key to recognizing a doesn't change after first iteration