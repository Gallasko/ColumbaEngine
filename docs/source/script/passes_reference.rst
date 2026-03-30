Bytecode Optimization Passes Reference
==================================================

.. note::
   This document is automatically generated from pass source code.
   Last updated: Auto-generated during build.

This reference documents all bytecode optimization passes available in the VM.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The VM includes **10 optimization passes** organized into categories:

* **Arithmetic**: 4 passes
* **Control Flow**: 3 passes
* **Memory**: 2 passes
* **Stack**: 1 passes

Quick Reference
---------------

.. list-table::
   :header-rows: 1
   :widths: 25 40 15 10 10

   * - Pass Name
     - Purpose
     - Category
     - Size Change
     - Multi-Pass
   * - :ref:`Basic Operator Local Indexing Pass <basic-operator-local-indexing-pass>`
     - Optimizes binary operations on local variables into speciali...
     - Arithmetic
     - Yes
     - No
   * - :ref:`Constant Folding Pass <constant-folding-pass>`
     - Evaluates constant expressions at compile time to eliminate ...
     - Arithmetic
     - Yes
     - Yes
   * - :ref:`Constant Variable Access Pass <constant-variable-access-pass>`
     - Propagates constant values assigned to variables
     - Arithmetic
     - Yes
     - Yes
   * - :ref:`Fuse Pop Operations Pass <fuse-pop-operations-pass>`
     - Combines multiple consecutive pop operations into single Pop...
     - Stack
     - Yes
     - Yes
   * - :ref:`Increment Optimization Pass <increment-optimization-pass>`
     - Converts x = x + 1 patterns into specialized increment instr...
     - Arithmetic
     - Yes
     - No
   * - :ref:`Long Jump Optimization Pass <long-jump-optimization-pass>`
     - Converts between short and long jump instructions based on o...
     - Control Flow
     - Yes
     - Yes
   * - :ref:`Popping Jump Optimization Pass <popping-jump-optimization-pass>`
     - Optimizes jump-if-false instructions followed by pops at bot...
     - Control Flow
     - Yes
     - Yes
   * - :ref:`Remove Define-Get Global Redundancy Pass <remove-define-get-global-redundancy-pass>`
     - Eliminates redundant global variable lookups after definitio...
     - Memory
     - Yes
     - Yes
   * - :ref:`Remove Useless Jump Pass <remove-useless-jump-pass>`
     - Removes jump instructions where the target is the immediatel...
     - Control Flow
     - Yes
     - Yes
   * - :ref:`Simplify Constant Pass <simplify-constant-pass>`
     - Replaces constant access patterns with optimized variants
     - Memory
     - Yes
     - No


Arithmetic
~~~~~~~~~~


.. _basic-operator-local-indexing-pass:

Basic Operator Local Indexing Pass
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Optimizes binary operations on local variables into specialized instructions

**Properties**:

* Class name: ``BasicOperatorLocalIndexingPass``
* Changes bytecode size: Yes
* Requires multiple passes: No

**Transformation Example**

Before::

    OP_Get_Local 0
    OP_Get_Local 1
    OP_Add

After::

    OP_AddLL 0 1

**Benefits**:

* Reduces 3 instructions to 1 (66%% reduction)
* Eliminates stack manipulation
* Direct register-style operations
* Significant performance gain in tight loops

**Notes**

Optimizes Add and Subtract operations on local variables.
Also handles mixed local/constant patterns.

**Source**: `basic_operator_local_indexing.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/basic_operator_local_indexing.h>`_


.. _constant-folding-pass:

Constant Folding Pass
^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Evaluates constant expressions at compile time to eliminate runtime computation

**Properties**:

* Class name: ``ConstantFoldingPass``
* Changes bytecode size: Yes
* Requires multiple passes: Yes

**Transformation Example**

Before::

    OP_Constant 5
    OP_Constant 3
    OP_Add

After::

    OP_Constant 8

**Benefits**:

* Eliminates runtime arithmetic operations
* Reduces instruction count by 66% for constant expressions (3 → 1 instruction)
* Smaller constant table through deduplication
* Better instruction cache utilization
* Cascading effect enables other optimizations

**Notes**

Requires multiple passes because folding can expose new opportunities.
Example: (5 + 3) + (2 + 4) needs two passes to fully fold to 14.
Supported operations:
- Arithmetic: +, -, *, /, % (binary), - (unary negation)
- Comparison: ==, !=, <, >, <=, >=
- Logical: and, or, not
- String: concatenation
Current limitation: Only folds to OP_Constant (not OP_LongConstant).
This limits constant table to 254 entries during optimization.

**Source**: `constant_folding.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/constant_folding.h>`_


.. _constant-variable-access-pass:

Constant Variable Access Pass
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Propagates constant values assigned to variables

**Properties**:

* Class name: ``ConstantVarAccess``
* Changes bytecode size: Yes
* Requires multiple passes: Yes

**Transformation Example**

Before::

    OP_Constant 42
    OP_Set_Local 0
    OP_Get_Local 0

After::

    OP_Constant 42
    OP_Set_Local 0
    OP_Constant 42

**Benefits**:

* Eliminates variable loads for constants
* Enables further constant folding

**Source**: `constant_var_access.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/constant_var_access.h>`_


.. _increment-optimization-pass:

Increment Optimization Pass
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Converts x = x + 1 patterns into specialized increment instructions

**Properties**:

* Class name: ``IncrementOptimizationPass``
* Changes bytecode size: Yes
* Requires multiple passes: No

**Transformation Example**

Before::

    OP_Get_Local 0
    OP_Constant 1
    OP_Add
    OP_Set_Local 0
    OP_Pop

After::

    OP_Short_Int 0
    OP_Post_Incr_Local

**Benefits**:

* Reduces instruction count from 5 to 2 (60% reduction)
* Eliminates constant table lookup
* Specialized opcode executes faster than generic arithmetic
* Critical for loop performance (for loops with i++)
* Better instruction cache utilization in tight loops

**Notes**

Recognizes the specific pattern of:
1. Loading a local variable
2. Adding constant value 1
3. Storing back to the same local variable
4. Popping the result (statement context)
The pass validates that the GET and SET target the same local variable
to avoid incorrect transformations. This pattern is extremely common in
for loops: for (var i = 0; i < n; i++)
Could be extended to support:
- Pre-increment (++i) pattern detection
- Decrement patterns (i--)
- Global variable increments

**Source**: `increment_optimization_pass.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/increment_optimization_pass.h>`_


Control Flow
~~~~~~~~~~~~


.. _long-jump-optimization-pass:

Long Jump Optimization Pass
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Converts between short and long jump instructions based on offset requirements

**Properties**:

* Class name: ``LongJumpOptimizationPass``
* Changes bytecode size: Yes
* Requires multiple passes: Yes

**Transformation Example**

Before::

    OP_Long_Jump <small offset>

After::

    OP_Jump <small offset>

**Benefits**:

* Reduce bytecode size by using short jumps when possible

**Source**: `long_jump_optimization_pass.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/long_jump_optimization_pass.h>`_


.. _popping-jump-optimization-pass:

Popping Jump Optimization Pass
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Optimizes jump-if-false instructions followed by pops at both locations into specialized popping jump instructions

**Properties**:

* Class name: ``PoppingJumpPass``
* Changes bytecode size: Yes
* Requires multiple passes: Yes

**Transformation Example**

Before::

    OP_Jump_If_False <offset>
    OP_Pop
    ...
    OP_Pop  ; at jump target

After::

    OP_Jump_If_False_Popping <offset>
    ...
    ; (both pops eliminated)

**Benefits**:

* Reduce bytecode size by eliminating redundant pop instructions
* Improve execution performance by combining jump and pop operations

**Source**: `popping_jump_pass.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/popping_jump_pass.h>`_


.. _remove-useless-jump-pass:

Remove Useless Jump Pass
^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Removes jump instructions where the target is the immediately following instruction

**Properties**:

* Class name: ``RemoveUselessJumpPass``
* Changes bytecode size: Yes
* Requires multiple passes: Yes

**Transformation Example**

Before::

    OP_Jump <offset to next instruction>
    <next instruction>

After::

    <next instruction>

**Benefits**:

* Reduce bytecode size by eliminating no-op jumps
* Improve execution performance by removing unnecessary control flow

**Source**: `remove_useless_jump_pass.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/remove_useless_jump_pass.h>`_


Memory
~~~~~~


.. _remove-define-get-global-redundancy-pass:

Remove Define-Get Global Redundancy Pass
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Eliminates redundant global variable lookups after definition

**Properties**:

* Class name: ``RemoveDefGetGlobalRedunduncy``
* Changes bytecode size: Yes
* Requires multiple passes: Yes

**Transformation Example**

Before::

    OP_Constant <value>
    OP_Define_Global <name>
    OP_Get_Global <name>

After::

    OP_Constant <value>
    OP_Define_Global_Non_Popping <name>

**Benefits**:

* Eliminates redundant global lookup
* Value stays on stack

**Source**: `remove_def_get_global_redunduncy.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/remove_def_get_global_redunduncy.h>`_


.. _simplify-constant-pass:

Simplify Constant Pass
^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Replaces constant access patterns with optimized variants

**Properties**:

* Class name: ``SimplifyConstantToShort``
* Changes bytecode size: Yes
* Requires multiple passes: No

**Transformation Example**

Before::

    OP_Constant <index>

After::

    OP_Short_Int 42

**Benefits**:

* Smaller bytecode
* Faster execution for small integers
* Reduced constant pool pressure

**Source**: `simplify_constant_pass.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/simplify_constant_pass.h>`_


Stack
~~~~~


.. _fuse-pop-operations-pass:

Fuse Pop Operations Pass
^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Combines multiple consecutive pop operations into single PopN instructions

**Properties**:

* Class name: ``FuseOpPop``
* Changes bytecode size: Yes
* Requires multiple passes: Yes

**Transformation Example**

Before::

    OP_Pop
    OP_Pop
    OP_Pop

After::

    OP_PopN 3

**Benefits**:

* Reduces instruction count (N pops → 1 instruction)
* Fewer instruction fetches and decodes
* Better branch prediction
* More efficient stack manipulation

**Notes**

Recognizes four patterns: Pop+Pop, Pop+PopN, PopN+Pop, PopN+PopN.
Requires multiple passes for consecutive fusions.

**Source**: `fuse_op_pop.h <https://github.com/Gallasko/ColumbaEngine/blob/main/Engine/Compiler/pass/fuse_op_pop.h>`_


Source Code
-----------

All optimization passes are located in `src/Engine/Compiler/pass/ <https://github.com/Gallasko/ColumbaEngine/tree/main/src/Engine/Compiler/pass>`_
