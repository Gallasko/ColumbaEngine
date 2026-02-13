Virtual Machine (VM) Internals
===============================

This document provides detailed information about the internal architecture of the Columba Virtual Machine, including its execution model, memory management, and optimization strategies.

Overview
--------

The Columba VM is a stack-based bytecode interpreter that executes compiled script code. It features:

* **Stack-based architecture**: Operations manipulate values on a runtime stack
* **NaN-boxing**: Efficient 64-bit value representation
* **Pool-based memory management**: Object pools with reference counting
* **Bytecode optimization**: Multi-pass optimization pipeline
* **Function pointer dispatch**: Fast opcode execution
* **Profiling support**: Built-in performance analysis tools

Architecture Components
-----------------------

Core VM Structure
~~~~~~~~~~~~~~~~~

The VM maintains several key components:

**Call Frames**
  Each function call creates a call frame that stores:

  * Closure reference (function + captured upvalues)
  * Instruction pointer (IP) - current bytecode position
  * Stack slots pointer - function's local variable base
  * Maximum of 64 nested call frames (``FRAMES_MAX``)

**Value Stack**
  A fixed-size stack (``FRAMES_MAX * 4096`` elements) storing all runtime values:

  * Function arguments and return values
  * Local variables
  * Temporary computation results
  * Optimized for single 64-bit MOV instructions

**Global Variables**
  Hash map storing global variable names and their values

**Pools System**
  Separate memory pools for each object type (see Memory Management section)

Execution Model
---------------

Bytecode Interpretation
~~~~~~~~~~~~~~~~~~~~~~~~

The VM executes bytecode through a function pointer dispatch system:

1. **Instruction Fetch**: Read opcode from current IP position
2. **Dispatch**: Call registered handler function for that opcode
3. **Execute**: Handler manipulates stack and updates VM state
4. **Advance**: Move IP to next instruction
5. **Repeat**: Continue until ``OP_Return`` or error

Example bytecode execution for addition::

    OP_Get_Local  0    # Push local variable 0 onto stack
    OP_Get_Local  1    # Push local variable 1 onto stack
    OP_Add             # Pop two values, add them, push result
    OP_Set_Local  2    # Pop result and store in local variable 2

Function Calls
~~~~~~~~~~~~~~

Function calls follow this sequence:

1. **Argument Setup**: Arguments pushed onto stack
2. **Call Instruction**: ``OP_Call`` with argument count
3. **Frame Creation**: New call frame allocated
4. **Closure Setup**: Function closure and captured upvalues loaded
5. **IP Update**: Instruction pointer set to function start
6. **Execution**: Function body executes
7. **Return**: ``OP_Return`` pops frame and restores previous context

Closure Mechanics
~~~~~~~~~~~~~~~~~

Closures capture variables from their enclosing scope:

**Upvalues**
  * Reference variables from parent scopes
  * Can be "open" (pointing to stack) or "closed" (moved to heap)
  * Automatically closed when stack frame exits

**Capture Process**::

    fun outer()
    {
        var x = 10;        # Local variable in outer's frame
        fun inner()
        {
            return x;      # Captures x as upvalue
        }
        return inner;
    }

When ``inner`` is created:

1. ``OP_Closure`` instruction creates closure object
2. Upvalue created pointing to ``x`` on outer's stack frame
3. When outer returns, upvalue is "closed" - ``x`` moved to heap
4. Inner function maintains reference to closed upvalue

Value Representation (NaN-Boxing)
----------------------------------

All values are represented as 64-bit integers using NaN-boxing, allowing efficient storage and type checking.

Encoding Scheme
~~~~~~~~~~~~~~~

* **Doubles**: Standard IEEE 754 representation (any non-NaN bit pattern)
* **Tagged Values**: Use IEEE 754 quiet NaN space::

    [Sign][0x7FF][1][unused][TAG(3)][INDEX/PAYLOAD(47)]

    Sign=0: Primary types (int, bool, string, closure, function, upvalue, class, native)
    Sign=1: Extended types (instance, bound_method, vector, small_string)
    Bits [49:47]: 3-bit type tag
    Bits [46:0]: 47-bit index or payload

Primary Types (Sign bit = 0)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------+-----+--------------------------------------------+
| Type      | Tag | Payload                                    |
+===========+=====+============================================+
| Integer   | 0   | 47-bit signed integer (±70 trillion)       |
+-----------+-----+--------------------------------------------+
| Boolean   | 1   | 0 (false) or 1 (true)                      |
+-----------+-----+--------------------------------------------+
| String    | 2   | Index into string pool                     |
+-----------+-----+--------------------------------------------+
| Closure   | 3   | Index into closure pool                    |
+-----------+-----+--------------------------------------------+
| Function  | 4   | Index into function pool                   |
+-----------+-----+--------------------------------------------+
| Upvalue   | 5   | Index into upvalue pool                    |
+-----------+-----+--------------------------------------------+
| Class     | 6   | Index into class pool                      |
+-----------+-----+--------------------------------------------+
| Native    | 7   | Index into native function pool            |
+-----------+-----+--------------------------------------------+

Extended Types (Sign bit = 1)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+--------------+-----+--------------------------------------------+
| Type         | Tag | Payload                                    |
+==============+=====+============================================+
| Instance     | 0   | Index into instance pool                   |
+--------------+-----+--------------------------------------------+
| BoundMethod  | 1   | Index into bound method pool               |
+--------------+-----+--------------------------------------------+
| Vector       | 2   | Index into vector pool                     |
+--------------+-----+--------------------------------------------+
| SmallString  | 3   | Inline string (up to 5 characters)         |
+--------------+-----+--------------------------------------------+

Benefits of NaN-Boxing
~~~~~~~~~~~~~~~~~~~~~~~

* **Space Efficient**: All values fit in 64 bits (8 bytes)
* **Fast Type Checking**: Single bit mask operation
* **Cache Friendly**: Compact representation improves cache locality
* **Register Passing**: Values fit in CPU registers
* **No Boxing Overhead**: No separate heap allocation for primitives

Memory Management
-----------------

Pool-Based Allocation
~~~~~~~~~~~~~~~~~~~~~

The VM uses separate memory pools for each object type:

* **String Pool**: ``ElementType`` objects (variable-length strings)
* **Closure Pool**: Function + captured upvalues
* **Function Pool**: Compiled function objects (``ObjFunction``)
* **Upvalue Pool**: Upvalue objects
* **Class Pool**: Class definitions (``Klass``)
* **Native Function Pool**: Native C++ function wrappers
* **Instance Pool**: Class instances (``ObjInstance``)
* **Bound Method Pool**: Instance + method pairs
* **Vector Pool**: Dynamic arrays (tables)

Pool Advantages
^^^^^^^^^^^^^^^

* **Fast Allocation**: No system malloc calls for common objects
* **Better Cache Locality**: Objects of same type stored contiguously
* **Reduced Fragmentation**: Fixed-size blocks per pool
* **Efficient Iteration**: Pools can be traversed linearly

Reference Counting
~~~~~~~~~~~~~~~~~~

Heap objects use reference counting for automatic memory management:

**Tracking**::

    Value trackNewValue(Value v)
    {
        pools.getRefCountVector(v)[index] = 1;
        return v;
    }

**Retaining**::

    Value retainValue(Value v)
    {
        if (!requiresRefCount(v)) return v;
        if (pools.isConstant(v)) return v;
        pools.getRefCountVector(v)[index]++;
        return v;
    }

**Releasing**::

    bool releaseValue(Value v)
    {
        if (!requiresRefCount(v)) return false;
        if (pools.isConstant(v)) return false;
        pools.getRefCountVector(v)[index]--;
        return (refCount == 0); // Should delete
    }

Reference Counting Rules
^^^^^^^^^^^^^^^^^^^^^^^^

* **Constants**: Never ref-counted (live forever in constant table)
* **Primitives**: No ref-counting needed (int, bool, double)
* **Stack Values**: Retained when pushed, released when popped
* **Global Variables**: Retained when stored, released on VM shutdown
* **Upvalues**: Special handling for open vs closed states

String Interning
~~~~~~~~~~~~~~~~

Strings are automatically interned to avoid duplicates:

* Hash table maps string content → pool index
* Duplicate strings reuse existing pool entry
* String comparison becomes integer comparison
* Significantly reduces memory usage

**Small String Optimization**: Strings ≤5 characters stored inline in the Value itself, avoiding pool allocation entirely.

Bytecode Format
---------------

Instruction Set
~~~~~~~~~~~~~~~

The VM executes a compact bytecode instruction set. Each instruction consists of:

* **Opcode**: 1 byte identifying the operation
* **Operands**: 0-4 bytes depending on instruction

Core Instructions
^^^^^^^^^^^^^^^^^

**Stack Operations**::

    OP_Constant <index>      # Push constant from constant table
    OP_LongConstant <i24>    # Push constant (24-bit index)
    OP_Pop                   # Pop and discard top of stack
    OP_PopN <count>          # Pop N values from stack

**Arithmetic**::

    OP_Add                   # Pop b, pop a, push a+b
    OP_Subtract              # Pop b, pop a, push a-b
    OP_Multiply              # Pop b, pop a, push a*b
    OP_Divide                # Pop b, pop a, push a/b
    OP_Negate                # Pop a, push -a

**Comparison**::

    OP_Equal                 # Pop b, pop a, push a==b
    OP_NotEqual              # Pop b, pop a, push a!=b
    OP_Greater               # Pop b, pop a, push a>b
    OP_GreaterEqual          # Pop b, pop a, push a>=b
    OP_Less                  # Pop b, pop a, push a<b
    OP_LessEqual             # Pop b, pop a, push a<=b

**Logical**::

    OP_Not                   # Pop a, push !a
    OP_And                   # Logical AND (short-circuit)
    OP_Or                    # Logical OR (short-circuit)

**Variables**::

    OP_Get_Local <index>     # Push local variable
    OP_Set_Local <index>     # Pop and store in local variable
    OP_Get_Global <index>    # Push global variable
    OP_Set_Global <index>    # Pop and store in global variable
    OP_Define_Global <idx>   # Pop and define new global variable

**Control Flow**::

    OP_Jump <offset>         # Unconditional jump (16-bit offset)
    OP_Long_Jump <offset>    # Unconditional jump (32-bit offset)
    OP_Jump_If_False <off>   # Jump if top of stack is false
    OP_Loop <offset>         # Jump backward (for loops)

**Functions**::

    OP_Call <argCount>       # Call function with N arguments
    OP_Return                # Return from current function
    OP_Closure <funcIdx>     # Create closure from function

**Classes and Objects**::

    OP_Class <nameIdx>       # Define new class
    OP_Get_Property <name>   # Get instance property
    OP_Set_Property <name>   # Set instance property
    OP_Method <nameIdx>      # Define method on class
    OP_Invoke <name><argc>   # Optimized method call

Metamethods
~~~~~~~~~~~

The VM supports metamethods that allow customization of property access behavior. Metamethods are special methods that intercept property operations.

**Supported Metamethods**:

``__get(self, propertyName)``
  Called when accessing a non-existent property. Returns the value to use.

``__set(self, propertyName, value)``
  Called when setting a property. Should return the value that was set.

**Metamethod Types**

Metamethods can be defined in two ways:

1. **Class Methods**: Defined on the class, available to all instances
2. **Dynamic Metamethods**: Stored as instance fields, allows per-instance customization

**Implementation Details**

Property Access (``OP_Get_Property``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When accessing a property (``instance.property``):

1. **Check real fields first**: If property exists in instance fields, return it immediately
2. **Check for ``__get``**: Look in class methods, then instance fields
3. **Call metamethod**: If found, invoke ``__get(instance, propertyName)``
4. **Check class methods**: Try to bind a class method
5. **Error**: If nothing found, runtime error

Property Assignment (``OP_Set_Property``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When setting a property (``instance.property = value``):

1. **Internal field bypass**: Fields starting with ``__`` bypass metamethods
2. **Internal access detection**: Direct field access from bound methods bypasses metamethods
3. **Check for ``__set``**: Look in class methods, then instance fields
4. **Call metamethod**: If found, invoke ``__set(instance, propertyName, value)``
5. **Direct assignment**: If no metamethod, store directly in instance fields

**Double-Underscore Convention**

Fields starting with ``__`` (double underscore) are treated as "internal" and **always bypass metamethods**. This convention prevents infinite recursion in metamethod implementations.

Example::

    class ProxyClass
    {
        init(x, y)
        {
            this.__data_x = x  // Direct field access, no __set called
            this.__data_y = y
        }
    }

    fun createProxy()
    {
        return fun(self, prop, value) {
            // Store data internally without triggering __set recursion
            if (prop == "x")
            {
                self.__data_x = value  // Bypasses __set
            }
        }
    }

    var obj = ProxyClass(10, 20)
    obj.__set = createProxy()
    obj.x = 100  // Calls __set, which stores to __data_x

**Internal Access Detection**

The VM detects when property access occurs from within a bound method of the same instance. In this case, metamethods are bypassed to allow direct field manipulation.

Detection criteria:

* Current frame is a bound method call (``stackBase == slots``)
* The accessed instance matches ``slots[0]`` (the receiver)
* Frame count > 1 (not the global script)

This allows methods to directly access their own instance fields without metamethod interception::

    class MyClass {
        init() {
            this.value = 0
        }

        increment() {
            this.value = this.value + 1  // Direct access, no __set called
        }
    }

**Stack Layout for Dynamic Metamethods**

When calling a dynamic metamethod (stored in instance field), the VM sets up the stack as::

    [closure] [instance] [arg1] [arg2] ...

For ``__get(self, propertyName)``:
  Stack: ``[__get_closure] [instance] [propertyName]``

For ``__set(self, propertyName, value)``:
  Stack: ``[__set_closure] [instance] [propertyName] [value]``

This differs from class method metamethods which use bound method calls.

**Index Operations**

Index operations (``instance[key]`` and ``instance[key] = value``) also support metamethods:

* ``OP_Get_Index``: Checks for ``__get`` metamethod if key not found
* ``OP_Set_Index``: Checks for ``__set`` metamethod before assignment
* Same double-underscore bypass rules apply

**Performance Considerations**

* Real fields are checked before metamethods (O(1) hash lookup)
* Metamethod lookup requires two hash lookups (class methods, then instance fields)
* Internal field access (``__`` prefix) is fastest (single hash lookup, no metamethod check)
* Bound method internal access bypasses metamethod checks entirely

**Tables**::

    OP_Build_Table <pairs>   # Create table from N key-value pairs
    OP_Build_Vector <count>  # Create vector from N values
    OP_Get_Index             # table[key] - pop key, pop table, push value
    OP_Set_Index             # table[key]=val - pop val, pop key, pop table

**Iterators**::

    OP_Get_Iterator          # Create iterator for table
    OP_Iterator_Next         # Advance iterator, push next key (or nil)
    OP_Table_Size            # Get table size
    OP_Table_At              # Get key at specific index

**Optimized Instructions**::

    OP_Incr_Local <idx>      # ++local (pre-increment)
    OP_Post_Incr_Local <idx> # local++ (post-increment)
    OP_Decr_Local <idx>      # --local (pre-decrement)
    OP_Post_Decr_Local <idx> # local-- (post-decrement)
    OP_AddLL <i1><i2>        # Add two local variables
    OP_SubtractLL <i1><i2>   # Subtract two local variables
    OP_Short_Int <value>     # Push small integer directly

Chunk Structure
~~~~~~~~~~~~~~~

A ``Chunk`` represents a compiled function's bytecode::

    struct Chunk
    {
        std::vector<uint8_t> code;           # Bytecode instructions
        std::vector<Value> constants;         # Constant pool
        std::vector<int> lines;               # Line numbers for debugging
        std::vector<std::string> importedModules;  # Module dependencies
    };

Function Objects
~~~~~~~~~~~~~~~~

Compiled functions are stored as ``ObjFunction`` objects::

    struct ObjFunction
    {
        Chunk chunk;             # Function's bytecode
        int arity;               # Number of parameters
        std::string name;        # Function name (for debugging)
        int upvalueCount;        # Number of captured variables
    };

Optimization System
-------------------

The VM includes a multi-pass bytecode optimization system that transforms bytecode after compilation but before execution.

See :doc:`optimizer` for detailed information about the optimization passes and how they work.

Profiling and Debugging
------------------------

VM Profiler
~~~~~~~~~~~

The VM includes a built-in profiler to measure bytecode execution performance:

**Enable profiling**::

    vm->enableProfiling();
    // ... run script ...
    vm->printProfilingReport(true);  // Sort by time
    vm->printProfilingBytecodeReport();

**Profiler Tracks**:

* Execution count per opcode
* Total time spent per opcode
* Time per opcode occurrence
* Hottest code paths

Debug Output
~~~~~~~~~~~~

Enable debug output for optimization passes::

    vm->enableOptimizationDebugging();

This prints:

* Bytecode before each pass
* Bytecode after each pass
* Changes made by each pass
* Pattern matches and rewrites

Disassembler
~~~~~~~~~~~~

The compiler includes a bytecode disassembler for debugging::

    disassembleChunk(vm, chunk, "FunctionName");

Output shows:

* Bytecode offset
* Source line number
* Opcode name
* Operands (with constant values)
* Jump targets

Performance Considerations
--------------------------

Hot Path Optimizations
~~~~~~~~~~~~~~~~~~~~~~

The VM is optimized for critical hot paths:

**Stack Operations**
  * Direct array access (no bounds checking in release builds)
  * Inline functions for push/pop
  * 64-bit values passed in registers
  * Cache-aligned stack array

**Instruction Dispatch**
  * Function pointer table for O(1) dispatch
  * Cached chunk data pointer
  * Cached chunk end pointer for fast loop exit
  * No switch statement overhead

**Value Operations**
  * NaN-boxing for fast type checks (single bitwise AND)
  * Reference counting for heap objects only
  * Constant values never ref-counted
  * Small string optimization avoids pool lookups

**Memory Access**
  * Pool-based allocation reduces cache misses
  * Contiguous storage for same-type objects
  * Reference counting vectors parallel to object pools
  * String interning reduces memory and comparison costs

Best Practices for VM Extension
--------------------------------

Adding Native Functions
~~~~~~~~~~~~~~~~~~~~~~~

Register native functions during VM initialization::

    vm->registerNative("myFunction", [](VM* vm, int argCount, Value* args) -> Value {
        // Validate argument count
        if (argCount != 2)
        {
            return makeBoolValue(false);
        }

        // Extract arguments
        int a = AS_INT(args[0]);
        int b = AS_INT(args[1]);

        // Perform operation
        int result = a + b;

        // Return result
        return makeIntValue(result);
    });

Creating Native Modules
~~~~~~~~~~~~~~~~~~~~~~~~

Define native modules with exported functions and variables::

    NativeModule mathModule;

    mathModule.exportedFunctions["sqrt"] = [](VM* vm, int argCount, Value* args) {
        if (argCount != 1 || !IS_DOUBLE(args[0]))
        {
            return makeIntValue(0); // Error handling
        }
        double result = std::sqrt(AS_DOUBLE(args[0]));
        return makeDoubleValue(result);
    };

    mathModule.exportedVariables["PI"] = ElementType(3.14159265358979323846);

    vm->addNativeModule("math", mathModule);

Module System
-------------

Import Resolution
~~~~~~~~~~~~~~~~~

When a script uses ``import "moduleName"``:

1. **Check Native Modules**: Look for registered native module
2. **Check File System**: Look for ``moduleName.pg`` or ``moduleName.pgc``
3. **Load and Execute**: Import module and add exports to global scope
4. **Cache**: Module loaded once, subsequent imports reuse cached version

Bytecode Caching
~~~~~~~~~~~~~~~~

Compiled bytecode can be saved and loaded:

**Compilation**::

    vm->interpretFromFile("script.pg", false, "script.pgc");
    // Creates script.pgc bytecode file

**Execution**::

    vm->interpretFromBytecodeFile("script.pgc");
    // Faster loading, no parsing/compilation

Cached bytecode includes:

* All bytecode instructions
* Constant table
* Function definitions
* Imported module list

Error Handling
--------------

The VM provides detailed error information:

**Compile Errors**
  * Syntax errors with line numbers
  * Type mismatches
  * Undefined variables

**Runtime Errors**
  * Full stack trace
  * Line numbers for each frame
  * Function names
  * Error message

Example error output::

    [ERROR] VM: Undefined variable 'foo'
    [ERROR] VM: [line 15] in myFunction
    [ERROR] VM: [line 23] in main

Error Recovery
~~~~~~~~~~~~~~

Runtime errors use ``setjmp/longjmp`` for fast unwinding:

1. Error detected during execution
2. ``longjmp`` to error handler
3. Stack automatically unwound
4. All reference counts decremented
5. VM state reset for next execution

Advanced Topics
---------------

JIT Considerations
~~~~~~~~~~~~~~~~~~

While the current VM is an interpreter, the architecture supports future JIT compilation:

* Bytecode designed for linear execution
* Pool indices can be patched for direct pointers
* Function pointer dispatch can be inlined
* Stack-based model maps well to register allocation

Threading Model
~~~~~~~~~~~~~~~

Each VM instance is single-threaded:

* No locks needed during execution
* Multiple VMs can run in separate threads
* Native functions must be thread-safe
* Shared data requires external synchronization

Extensions
~~~~~~~~~~

The VM can be extended with:

* Custom native modules
* Additional bytecode instructions
* New optimization passes
* Custom object types in pools
* Specialized calling conventions

Further Reading
---------------

* :doc:`api` - Complete language API reference
* :doc:`optimizer` - Bytecode optimization system
* :doc:`overview` - Language overview and features

Related Files
-------------

Source code locations:

* VM implementation: `vm.cpp <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/vm.cpp>`_
* VM header: `vm.h <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/vm.h>`_
* NaN-boxing: `value_nanbox.h <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/value_nanbox.h>`_
* Chunk structure: `chunk.h <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/chunk.h>`_
* Memory pools: `vmpools.h <https://github.com/Gallasko/ColumbaEngine/blob/main/src/Engine/Compiler/vmpools.h>`_