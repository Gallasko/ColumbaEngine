Bytecode Optimizer
==================

The PgEngine bytecode optimizer is a multi-pass optimization system that transforms bytecode after compilation but before execution. It performs pattern-based transformations to improve runtime performance and reduce bytecode size.

Overview
--------

The optimization system consists of:

* **PassManager**: Orchestrates multiple optimization passes
* **BytecodePass**: Base class for individual optimization passes
* **BytecodeRewriter**: Pattern matching and bytecode transformation engine
* **Optimization Passes**: Specific optimization implementations

The optimizer runs automatically after compilation (unless disabled) and can apply multiple iterations of each pass until no more optimizations are found.

Architecture
------------

PassManager
~~~~~~~~~~~

The ``PassManager`` coordinates the execution of optimization passes:

**Responsibilities**:

* Maintains ordered list of optimization passes
* Executes passes in sequence
* Handles multi-pass iterations
* Provides debug output for optimization process
* Manages the shared ``BytecodeRewriter`` instance

**Configuration**::

    // Enable/disable optimization
    vm->enableBytecodeOptimization();
    vm->disableBytecodeOptimization();

    // Enable debug output to see transformations
    vm->enableOptimizationDebugging();

    // List all registered passes
    vm->listOptimizationPasses();

BytecodePass Interface
~~~~~~~~~~~~~~~~~~~~~~

All optimization passes inherit from ``BytecodePass``:

.. code-block:: cpp

    class BytecodePass {
    public:
        virtual ~BytecodePass() = default;

        // Pass identification
        virtual std::string getName() const = 0;

        // Execute the optimization pass
        virtual bool runPass(Chunk& chunk, BytecodeRewriter* rewriter) = 0;

        // Indicates if pass changes bytecode size
        virtual bool changesSize() const { return false; }

        // Indicates if pass should run multiple times
        virtual bool requiresMultiplePasses() const { return false; }
    };

BytecodeRewriter
~~~~~~~~~~~~~~~~

The ``BytecodeRewriter`` provides powerful pattern matching and transformation capabilities:

**Pattern Elements**:

* ``match(opcode)``: Match specific opcode
* ``anyOf(opcodes)``: Match any of multiple opcodes
* ``wildcard()``: Match any opcode
* Predefined groups: ``constant()``, ``load()``, ``store()``, ``jump()``, etc.

**Advanced Features**:

* Capture matched instructions for analysis
* Lambda-based transformations
* Conditional pattern matching
* Automatic jump offset adjustment
* Safe bytecode rewriting

Optimization Passes
-------------------

The VM includes several built-in optimization passes that run by default.

Constant Folding Pass
~~~~~~~~~~~~~~~~~~~~~~

**Purpose**: Evaluate constant expressions at compile time

**Name**: ``ConstantFoldingPass``

**Transformations**:

1. **Arithmetic on Constants**::

    # Before
    OP_Constant 5
    OP_Constant 3
    OP_Add

    # After
    OP_Constant 8

2. **Comparison on Constants**::

    # Before
    OP_Constant 10
    OP_Constant 5
    OP_Greater

    # After
    OP_True

3. **Logical Operations**::

    # Before
    OP_True
    OP_False
    OP_And

    # After
    OP_False

**Benefits**:

* Reduces instruction count
* Eliminates runtime computation
* Smaller constant table
* Better cache utilization

**Properties**:

* Changes bytecode size: Yes
* Requires multiple passes: Yes (for nested expressions)

Increment Optimization Pass
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Purpose**: Convert addition patterns to specialized increment instructions

**Name**: ``IncrementOptimizationPass``

**Transformations**:

**Pre-increment pattern**::

    # Before
    OP_Get_Local 0
    OP_Constant 1
    OP_Add
    OP_Set_Local 0
    OP_Pop

    # After
    OP_Incr_Local 0

**Post-increment pattern**::

    # Before
    OP_Get_Local 0
    OP_Constant 1
    OP_Add
    OP_Set_Local 0

    # After
    OP_Short_Int 0
    OP_Post_Incr_Local 0

**Benefits**:

* 5 instructions → 1-2 instructions
* Faster execution (specialized opcode)
* Smaller bytecode size
* Common in loops

Fuse OP_Pop Pass
~~~~~~~~~~~~~~~~

**Purpose**: Combine multiple consecutive pop operations

**Name**: ``FuseOpPop``

**Transformations**:

**Two pops**::

    # Before
    OP_Pop
    OP_Pop

    # After
    OP_PopN 2

**Pop + PopN**::

    # Before
    OP_Pop
    OP_PopN 3

    # After
    OP_PopN 4

**PopN + PopN**::

    # Before
    OP_PopN 5
    OP_PopN 3

    # After
    OP_PopN 8

**Benefits**:

* Reduces instruction count
* Better branch prediction
* Fewer instruction fetches
* Smaller bytecode

**Properties**:

* Changes bytecode size: Yes
* Requires multiple passes: Yes (for consecutive fusions)

Long Jump Optimization Pass
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Purpose**: Convert short jumps to long jumps when necessary, or long jumps to short jumps when possible

**Name**: ``LongJumpOptimizationPass``

**Transformations**::

    # Before (jump offset > 65535)
    OP_Jump 0x1234

    # After
    OP_Long_Jump 0x00012345

    # Before (long jump with offset < 65535)
    OP_Long_Jump 0x0000FFFF

    # After
    OP_Jump 0xFFFF

**Benefits**:

* Correct jumps for large functions
* Smaller bytecode when long jumps unnecessary
* Handles bytecode size changes from other passes

Remove Redundant Get/Define Global Pass
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Purpose**: Optimize patterns where globals are defined and immediately accessed

**Name**: ``RemoveDefGetGlobalRedundancy``

**Transformations**::

    # Before
    OP_Constant <value>
    OP_Define_Global <name>
    OP_Get_Global <name>

    # After
    OP_Constant <value>
    OP_Define_Global_Non_Popping <name>

**Benefits**:

* Eliminates redundant global lookup
* One less instruction
* Value stays on stack

Simplify Constant Pass
~~~~~~~~~~~~~~~~~~~~~~~

**Purpose**: Replace constant access patterns with optimized variants

**Name**: ``SimplifyConstantPass``

**Transformations**:

**Small integers**::

    # Before
    OP_Constant <index of integer 0-255>

    # After
    OP_Short_Int <value>

**Constant globals**::

    # Before
    OP_Constant <name>
    OP_Define_Global

    # After
    OP_Define_Constant_Global <name>

**Benefits**:

* Smaller bytecode
* Faster execution (no constant table lookup)
* Reduced constant pool pressure

Basic Operator Local Indexing Pass
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Purpose**: Optimize binary operations on local variables

**Name**: ``BasicOperatorLocalIndexing``

**Transformations**:

**Addition**::

    # Before
    OP_Get_Local 0
    OP_Get_Local 1
    OP_Add

    # After
    OP_AddLL 0 1

**Subtraction**::

    # Before
    OP_Get_Local 0
    OP_Get_Local 1
    OP_Subtract

    # After
    OP_SubtractLL 0 1

**Mixed constant/local**::

    # Before
    OP_Get_Local 0
    OP_Constant 5
    OP_Subtract

    # After
    OP_SubtractLC 0 5

**Benefits**:

* 3 instructions → 1 instruction
* Direct register-style operations
* Eliminates stack manipulation
* Significant performance gain in tight loops

Constant Variable Access Pass
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Purpose**: Optimize access to variables holding constant values

**Name**: ``ConstantVarAccess``

**Transformations**:

Detects variables that are assigned constant values and never modified, then propagates those constants directly.

Pattern Matching Engine
-----------------------

Advanced Pattern Matching
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``BytecodeRewriter`` provides a powerful pattern matching API:

**Pattern Elements**:

.. code-block:: cpp

    // Match specific opcode
    PatternElement::match(OpCode::OP_Add)

    // Match with capture
    PatternElement::match(OpCode::OP_Constant, true)

    // Match any of multiple opcodes
    PatternElement::anyOf({OP_Add, OP_Subtract, OP_Multiply})

    // Predefined groups
    PatternElement::constant()      // OP_Constant or OP_LongConstant
    PatternElement::load()          // OP_Get_Local or OP_Get_Global
    PatternElement::store()         // OP_Set_Local or OP_Set_Global
    PatternElement::jump()          // Any jump instruction
    PatternElement::increment()     // Any increment instruction

**Captured Instructions**:

Captured instructions provide access to:

* Opcode
* Operands (raw bytes)
* Bytecode offset
* Helper methods (e.g., ``getConstantIndex()``)

Example Pattern Matching
~~~~~~~~~~~~~~~~~~~~~~~~~

**Simple pattern**:

.. code-block:: cpp

    std::vector<PatternElement> pattern = {
        PatternElement::match(OpCode::OP_Pop),
        PatternElement::match(OpCode::OP_Pop),
    };

    rewriter->addAdvancedRule(pattern,
        [](const std::vector<CapturedInstruction>&) {
            std::vector<uint8_t> newCode;
            newCode.push_back(static_cast<uint8_t>(OpCode::OP_PopN));
            newCode.push_back(2);
            return newCode;
        });

**Complex pattern with capture and condition**:

.. code-block:: cpp

    // Detect: GET_LOCAL x, CONSTANT 1, ADD, SET_LOCAL x
    std::vector<PatternElement> pattern = {
        PatternElement::match(OpCode::OP_Get_Local, true),    // Capture
        PatternElement::constant(true),                       // Capture
        PatternElement::match(OpCode::OP_Add),
        PatternElement::match(OpCode::OP_Set_Local, true),    // Capture
    };

    auto condition = [&chunk](const std::vector<CapturedInstruction>& captured) {
        // Verify same local variable
        if (captured[0].operands[0] != captured[2].operands[0]) {
            return false;
        }

        // Verify constant is 1
        Value constValue = chunk.constants[captured[1].getConstantIndex()];
        return IS_INT(constValue) && AS_INT(constValue) == 1;
    };

    auto transform = [](const std::vector<CapturedInstruction>& captured) {
        std::vector<uint8_t> newCode;
        newCode.push_back(static_cast<uint8_t>(OpCode::OP_Incr_Local));
        newCode.push_back(captured[0].operands[0]); // Local index
        return newCode;
    };

    rewriter->addAdvancedRule(pattern, transform, condition);

Jump Offset Handling
~~~~~~~~~~~~~~~~~~~~

The rewriter automatically handles jump offset adjustments when bytecode size changes:

* Tracks all jump targets before rewriting
* Adjusts forward and backward jumps
* Handles both short (16-bit) and long (32-bit) jumps
* Converts between short/long jumps as needed
* Updates loop instructions (``OP_Loop``)

This ensures that control flow remains correct after optimization.

Writing Custom Passes
----------------------

Creating a New Pass
~~~~~~~~~~~~~~~~~~~

1. **Inherit from BytecodePass**:

.. code-block:: cpp

    class MyOptimizationPass : public BytecodePass {
    public:
        std::string getName() const override {
            return "MyOptimizationPass";
        }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter) override;

        bool changesSize() const override { return true; }
        bool requiresMultiplePasses() const override { return false; }
    };

2. **Implement the Pass Logic**:

.. code-block:: cpp

    bool MyOptimizationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        // Define pattern to match
        std::vector<PatternElement> pattern = {
            // Your pattern here
        };

        // Define transformation
        auto transform = [](const std::vector<CapturedInstruction>& captured) {
            std::vector<uint8_t> newCode;
            // Generate optimized bytecode
            return newCode;
        };

        // Optional: Add condition
        auto condition = [&chunk](const std::vector<CapturedInstruction>& captured) {
            // Return true if transformation should apply
            return true;
        };

        // Register rule with rewriter
        rewriter->addAdvancedRule(pattern, transform, condition);

        // Apply rewrite rules
        return rewriter->rewrite(chunk);
    }

3. **Register the Pass**:

.. code-block:: cpp

    vm->addOptimizationPass(std::make_unique<MyOptimizationPass>());

Best Practices
~~~~~~~~~~~~~~

**Pattern Design**:

* Keep patterns specific enough to avoid false matches
* Use captures only when needed (better performance)
* Leverage predefined pattern groups
* Test patterns with edge cases

**Transformation Safety**:

* Verify operand bounds before access
* Check constant table indices are valid
* Ensure generated bytecode is well-formed
* Consider side effects of original instructions

**Conditions**:

* Use conditions to validate semantic correctness
* Check for same variable indices
* Verify constant values
* Ensure no live values are discarded

**Multiple Passes**:

* Set ``requiresMultiplePasses()`` if pass can enable more optimizations
* PassManager limits iterations to prevent infinite loops
* Each iteration should make progress

**Size Changes**:

* Set ``changesSize()`` if instruction count changes
* Rewriter handles jump offset updates automatically
* Be aware of constant table impact

Debugging Optimizations
-----------------------

Enable Debug Output
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    vm->enableOptimizationDebugging();

This prints:

* Original bytecode before all passes
* Bytecode after each pass
* Pass names and iteration counts
* Which passes made changes

Example output::

    [INFO] PassManager: Running 6 passes
    [INFO] PassManager: Running pass: ConstantFoldingPass

    === BYTECODE BEFORE ConstantFoldingPass ===
    0000    1 OP_Constant         0 '5'
    0002    1 OP_Constant         1 '3'
    0004    1 OP_Add
    0005    1 OP_Debug_Print

    === BYTECODE AFTER ConstantFoldingPass ===
    0000    1 OP_Constant         0 '8'
    0002    1 OP_Debug_Print

    [INFO] PassManager: Pass ConstantFoldingPass made changes (iteration 1)

Manual Pass Execution
~~~~~~~~~~~~~~~~~~~~~

Run specific passes for testing:

.. code-block:: cpp

    bool changed = passManager.runPass("ConstantFoldingPass", chunk);

Disassemble Bytecode
~~~~~~~~~~~~~~~~~~~~

View bytecode at any point:

.. code-block:: cpp

    disassembleChunk(vm, chunk, "After My Pass");

Verification
~~~~~~~~~~~~

After optimization, verify:

* All jump targets are valid
* Constant indices are in bounds
* Stack depth is consistent
* Local variable indices are valid
* Line number information preserved

Performance Impact
------------------

Optimization Overhead
~~~~~~~~~~~~~~~~~~~~~

* Pattern matching: ~1-5ms per 10,000 instructions
* Constant folding: ~10-20% bytecode size reduction
* Overall optimization: ~5-10ms for typical scripts
* Significant runtime performance gains (10-50% faster)

Typical Improvements
~~~~~~~~~~~~~~~~~~~~

**Bytecode Size**:

* 10-30% smaller after all passes
* Fewer instructions to fetch and decode
* Better instruction cache utilization

**Execution Speed**:

* 10-50% faster depending on code patterns
* Loops benefit most (increment optimization)
* Arithmetic-heavy code sees large gains
* Function call overhead unchanged

**Memory Usage**:

* Smaller constant tables
* Fewer temporary stack values
* Better cache locality

When to Disable
~~~~~~~~~~~~~~~

Consider disabling optimization for:

* Very short scripts (overhead > benefit)
* Scripts run once (no amortization of compilation cost)
* Debugging (clearer bytecode correspondence to source)
* Testing (verify optimizer correctness)

Example: Complete Custom Pass
------------------------------

Here's a complete example implementing a pass that optimizes ``x = x * 2`` to a left shift:

.. code-block:: cpp

    class MultiplyByTwoPass : public BytecodePass {
    public:
        std::string getName() const override {
            return "MultiplyByTwoPass";
        }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter) override {
            if (!rewriter) return false;

            // Pattern: GET_LOCAL x, CONSTANT 2, MULTIPLY, SET_LOCAL x
            std::vector<PatternElement> pattern = {
                PatternElement::match(OpCode::OP_Get_Local, true),
                PatternElement::constant(true),
                PatternElement::match(OpCode::OP_Multiply),
                PatternElement::match(OpCode::OP_Set_Local, true),
            };

            // Condition: same local, constant is 2
            auto condition = [&chunk](const std::vector<CapturedInstruction>& captured) {
                // Same local variable?
                if (captured[0].operands[0] != captured[2].operands[0]) {
                    return false;
                }

                // Constant is 2?
                Value constValue = chunk.constants[captured[1].getConstantIndex()];
                if (!IS_INT(constValue)) return false;
                return AS_INT(constValue) == 2;
            };

            // Transform: use custom left shift operation
            auto transform = [](const std::vector<CapturedInstruction>& captured) {
                std::vector<uint8_t> newCode;
                // OP_LeftShift_Local_1 <local_index>
                newCode.push_back(static_cast<uint8_t>(OpCode::OP_LeftShift_Local_1));
                newCode.push_back(captured[0].operands[0]);
                return newCode;
            };

            rewriter->addAdvancedRule(pattern, transform, condition);
            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }
        bool requiresMultiplePasses() const override { return false; }
    };

    // Register the pass
    vm->addOptimizationPass(std::make_unique<MultiplyByTwoPass>());

Pass Execution Order
--------------------

The default pass order is optimized for maximum benefit:

1. **Simplify Constant Pass**: Enable other optimizations
2. **Constant Folding Pass**: Reduce expressions
3. **Remove Redundant Get/Define Global**: Simplify global access
4. **Basic Operator Local Indexing**: Optimize arithmetic
5. **Increment Optimization Pass**: Optimize loops
6. **Fuse OP_Pop Pass**: Combine stack operations
7. **Constant Variable Access**: Propagate constants
8. **Long Jump Optimization Pass**: Fix jumps (always last)

Pass order matters because:

* Early passes enable later passes
* Size-changing passes affect jump offsets
* Jump optimization must run last

Future Enhancements
-------------------

Potential Future Optimizations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Dead Code Elimination**: Remove unreachable code
* **Loop Invariant Code Motion**: Hoist constant expressions from loops
* **Inline Small Functions**: Eliminate call overhead
* **Tail Call Optimization**: Convert recursion to iteration
* **Peephole Optimizations**: More specialized instruction patterns
* **Global Value Numbering**: Eliminate redundant computations
* **Register Allocation**: For potential JIT compiler

See Also
--------

* :doc:`vm_internals` - Detailed VM architecture
* :doc:`api` - Script language API reference
* :doc:`overview` - Language overview

Related Files
-------------

Source code locations:

* Pass manager: `bytecode_pass.cpp <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/bytecode_pass.cpp>`_
* Pass interface: `bytecode_pass.h <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/bytecode_pass.h>`_
* Bytecode rewriter: `bytecode_rewriter.cpp <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/bytecode_rewriter.cpp>`_
* Rewriter header: `bytecode_rewriter.h <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/bytecode_rewriter.h>`_
* Optimization passes directory: `pass/ <https://github.com/Gallasko/ColumbaEngine/tree/main/src/Engine/Compiler/pass>`_
