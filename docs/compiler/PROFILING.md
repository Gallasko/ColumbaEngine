# VM Bytecode Profiling

The PgCompiler VM includes a built-in profiler that tracks execution count and timing for each bytecode instruction **by position** in the bytecode stream.

## Features

- **Per-instruction profiling**: Tracks each instruction position separately (not just by opcode type)
- **Execution counting**: Records how many times each instruction is executed
- **Timing**: Measures nanosecond-precision timing for each instruction
- **Multiple report formats**: View results sorted by time, execution count, or bytecode order
- **Zero overhead when disabled**: Profiling only adds overhead when explicitly enabled

## Usage

### C++ API

```cpp
VM vm;

// Enable profiling
vm.enableProfiling();

// Run your code
vm.interpretFromFile("myprogram.pg");

// Print reports
vm.printProfilingReport(true);        // Sort by total time
vm.printProfilingReport(false);       // Sort by execution count
vm.printProfilingBytecodeReport();    // Show in bytecode order

// Reset profiling data
vm.resetProfiling();

// Disable profiling
vm.disableProfiling();
```

### Example Output

#### Bytecode Order Report
```
=== VM Bytecode Execution Profile (by instruction order) ===
Total instructions executed: 1234567
Total execution time: 45.678 ms

Instruction-by-instruction breakdown:

Offset  Opcode                       Exec Count      Total (μs)      Avg (ns)
-----------------------------------------------------------------------------------
0       OP_Define_Constant_Global    1               15.234          15234.00
3       OP_Constant                  1               2.456           2456.00
5       OP_Constant                  1               2.123           2123.00
...
109     OP_Get_Local                 50000           1234.567        24.69
111     OP_Get_Local                 50000           1198.234        23.96
...
```

#### Top Instructions by Time
```
=== VM Bytecode Profiling Report ===
Total instructions executed: 1234567
Total execution time: 45.678 ms
Average time per instruction: 37.01 ns

Top instructions by total time:

Offset  Opcode                   Count       Total (ms)      Avg (ns)        % Time
-----------------------------------------------------------------------------------
109     OP_Get_Local             50000       1.235           24.69           2.70%
111     OP_Get_Local             50000       1.198           23.96           2.62%
88      OP_Call                  10000       5.234           523.40          11.46%
132     OP_Call                  10000       4.987           498.70          10.92%
```

## Use Cases

### 1. Finding Hot Spots

Identify which instructions consume the most time:

```cpp
vm.enableProfiling();
vm.interpretFromFile("program.pg");
vm.printProfilingReport(true);  // Sort by time
```

Look for:
- High total time = optimization target
- High execution count in loops = consider loop unrolling
- Expensive operations (OP_Call, OP_Get_Index) = consider caching

### 2. Validating Optimizations

Compare before and after optimization:

```cpp
// Before
vm.enableProfiling();
vm.interpretFromFile("unoptimized.pg");
vm.printProfilingReport();

vm.resetProfiling();

// After
vm.interpretFromFile("optimized.pg");
vm.printProfilingReport();
```

### 3. Loop Analysis

Use bytecode order view to see loop execution patterns:

```cpp
vm.enableProfiling();
vm.interpretFromFile("loop_heavy.pg");
vm.printProfilingBytecodeReport();
```

Look for:
- Instructions with very high execution counts = loop body
- Adjacent instructions with similar counts = tight loop
- Jumps with high counts = loop back edges

### 4. Finding Optimization Candidates

Instructions to watch for optimization:

| Pattern | Problem | Solution |
|---------|---------|----------|
| Multiple `OP_Constant` with same value | Redundant constant loads | Constant reuse pass |
| `OP_Constant` + `OP_Constant` + `OP_Add` | Constant folding opportunity | Constant folding pass |
| `OP_Get_Local` + `OP_Constant` + `OP_Add` + `OP_Set_Local` (same local) | Manual increment | Use `OP_Incr_Local` |
| Many `OP_Constant` followed by `OP_Build_Vector` | Building constant vector at runtime | Pre-build constant vector |
| High-count `OP_Jump_If_False` + `OP_Pop` pairs | Extra pop operations | Use `OP_Pop_Jump_If_False` |

## Performance Notes

- Profiling adds ~50-100ns overhead per instruction when enabled
- Use only in development/profiling builds
- For production, ensure profiling is disabled
- Profiler uses `std::chrono::high_resolution_clock` for timing

## Integration with Test Suite

```cpp
TEST_F(PerformanceTest, ProfileComplexScript)
{
    VM vm;
    vm.enableProfiling();

    ASSERT_EQ(vm.interpretFromFile("complex_script.pg"), InterpretResult::OK);

    // Verify no instruction takes more than 1000ns on average
    auto results = vm.profiler.getResultsSortedByOffset();
    for (const auto& profile : results)
    {
        EXPECT_LT(profile.averageTimeNs(), 1000.0)
            << "Instruction at offset " << profile.instructionOffset
            << " (" << profile.opcodeName << ") is too slow";
    }

    vm.printProfilingBytecodeReport();
}
```
