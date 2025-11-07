# Comprehensive Test Plan for PgCompiler

## Current Coverage Analysis

### ✅ Already Tested (11 tests)
- Basic arithmetic: +, -, *, /
- Boolean literals: true, false
- Boolean NOT: !
- Equality comparison: ==
- Operator precedence
- Parentheses
- Syntax errors
- Runtime errors

### ❌ Missing Coverage
Based on the compiler analysis, we need **93 additional tests** to cover all features.

---

## TEST PLAN: 104 Total Tests

### CATEGORY 1: COMPARISON OPERATORS (5 new tests)
**Current:** 1/7 operators tested

| Test Name | Description | Priority |
|-----------|-------------|----------|
| `inequality_comparison` | Test != operator | HIGH |
| `less_than_comparison` | Test < and <= operators | HIGH |
| `greater_than_comparison` | Test > and >= operators | HIGH |
| `comparison_chains` | Test multiple comparisons: 1 < 2 < 3 | MEDIUM |
| `comparison_edge_cases` | Test NaN, infinity comparisons | LOW |

---

### CATEGORY 2: LOGICAL OPERATORS (3 new tests)
**Current:** 0/2 operators tested

| Test Name | Description | Priority |
|-----------|-------------|----------|
| `logical_and` | Test `and` operator with short-circuit | HIGH |
| `logical_or` | Test `or` operator with short-circuit | HIGH |
| `logical_combinations` | Test complex: `(a or b) and (c or d)` | MEDIUM |

---

### CATEGORY 3: UNARY OPERATORS (4 new tests)
**Current:** 1/4 operators tested

| Test Name | Description | Priority |
|-----------|-------------|----------|
| `unary_negation` | Test -expr with numbers | HIGH |
| `unary_negation_edge_cases` | Test --5, -(-5), -(0) | MEDIUM |
| `double_negation` | Test !!expr for boolean coercion | LOW |
| `complex_unary` | Test -(-(-5)), !(!true) | LOW |

---

### CATEGORY 4: VARIABLES (12 new tests)
**Current:** 0/? variable features tested

#### Declaration & Assignment
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `variable_declaration` | var x; var y = 5; | HIGH |
| `variable_assignment` | x = 10; x = x + 1; | HIGH |
| `multiple_declarations` | var a = 1; var b = 2; var c = 3; | HIGH |
| `variable_shadowing` | Local var shadows global | MEDIUM |

#### Scoping
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `local_scope` | Block scope: { var x = 1; } | HIGH |
| `global_scope` | Global variable access | HIGH |
| `nested_scope` | Nested blocks with variables | MEDIUM |
| `scope_lifetime` | Variable exists only in scope | MEDIUM |

#### Increment/Decrement
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `prefix_increment` | ++x (returns new value) | HIGH |
| `postfix_increment` | x++ (returns old value) | HIGH |
| `prefix_decrement` | --x (returns new value) | HIGH |
| `postfix_decrement` | x-- (returns old value) | HIGH |

---

### CATEGORY 5: CONTROL FLOW (12 new tests)

#### If Statements
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `if_statement` | Basic if with true condition | HIGH |
| `if_else_statement` | If with else branch | HIGH |
| `if_false_condition` | If with false condition (skips body) | HIGH |
| `nested_if_statements` | If inside if | MEDIUM |
| `if_else_if_chain` | if...else if...else if...else | MEDIUM |

#### While Loops
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `while_loop` | Basic while loop | HIGH |
| `while_loop_zero_iterations` | While with false condition | HIGH |
| `while_loop_break_condition` | Loop that modifies condition variable | MEDIUM |
| `nested_while_loops` | While inside while | MEDIUM |

#### For Loops
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `for_loop` | Basic for loop | HIGH |
| `for_loop_empty_parts` | for (;;) infinite loop | MEDIUM |
| `for_loop_scope` | Iterator variable scope | MEDIUM |

---

### CATEGORY 6: FUNCTIONS (15 new tests)

#### Declaration & Call
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `function_declaration` | fun add(a, b) { return a + b; } | HIGH |
| `function_call` | Calling function with args | HIGH |
| `function_no_params` | fun greet() { ... } | HIGH |
| `function_no_return` | Function without return (implicit 0) | HIGH |
| `function_multiple_returns` | Different return paths | MEDIUM |

#### Parameters & Arguments
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `function_many_params` | Function with 10 parameters | MEDIUM |
| `function_argument_evaluation` | Args evaluated left-to-right | LOW |

#### Closures
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `closure_basic` | Function captures outer variable | HIGH |
| `closure_nested` | Nested function closures | HIGH |
| `closure_multiple_captures` | Capture multiple variables | MEDIUM |

#### Recursion
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `recursive_function` | Factorial or Fibonacci | HIGH |
| `mutual_recursion` | Two functions calling each other | MEDIUM |

#### First-Class Functions
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `function_as_value` | Assign function to variable | MEDIUM |
| `function_as_argument` | Pass function to another function | MEDIUM |
| `function_return_function` | Return function from function | LOW |

---

### CATEGORY 7: CLASSES & OBJECTS (18 new tests)

#### Class Declaration
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `class_declaration` | class Dog { } | HIGH |
| `class_with_methods` | class with method definitions | HIGH |
| `class_initializer` | class with init() method | HIGH |
| `class_empty` | Empty class declaration | MEDIUM |

#### Instance Creation & Fields
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `instance_creation` | var dog = Dog(); | HIGH |
| `instance_fields` | dog.name = "Rex"; | HIGH |
| `instance_field_access` | __dprint(dog.name); | HIGH |
| `instance_undefined_field` | Access non-existent field | MEDIUM |
| `instance_many_fields` | Instance with 10+ fields | LOW |

#### Methods
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `method_call` | dog.bark(); | HIGH |
| `method_with_params` | dog.setName("Max"); | HIGH |
| `method_return_value` | dog.getAge() returns value | HIGH |
| `method_access_fields` | Method uses `this.field` | HIGH |

#### This Keyword
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `this_in_method` | this refers to instance | HIGH |
| `this_in_init` | this in initializer | HIGH |

#### Method Binding
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `bound_method` | var fn = dog.bark; fn(); | MEDIUM |
| `method_as_callback` | Pass method to function | LOW |

#### Inheritance (if implemented)
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `class_inheritance` | Check if inheritance exists | LOW |

---

### CATEGORY 8: STRINGS (6 new tests)
**Current:** 0/? string features tested

| Test Name | Description | Priority |
|-----------|-------------|----------|
| `string_literals` | "hello", 'world' | HIGH |
| `string_concatenation` | "hello" + " " + "world" | HIGH |
| `empty_string` | "" | MEDIUM |
| `string_escape_sequences` | "\n", "\t", "\"" | MEDIUM |
| `string_comparison` | "a" == "a", "a" != "b" | MEDIUM |
| `string_in_variables` | var s = "test"; | HIGH |

---

### CATEGORY 9: EDGE CASES & TRICKY TESTS (15 new tests)

#### Numeric Edge Cases
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `division_by_zero` | 1 / 0 (runtime error?) | HIGH |
| `integer_overflow` | Very large numbers | MEDIUM |
| `float_precision` | 0.1 + 0.2 == 0.3 | LOW |
| `negative_zero` | -0 behavior | LOW |

#### Expression Complexity
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `deeply_nested_expressions` | ((((1 + 2) * 3) - 4) / 5) | MEDIUM |
| `long_expression_chain` | 1+2+3+4+5+6+7+8+9+10 | MEDIUM |
| `mixed_type_operations` | 5 + 3.14 | HIGH |

#### Scope Traps
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `variable_use_before_declaration` | x = 5; var x; (error) | HIGH |
| `local_variable_in_own_initializer` | var x = x + 1; (error) | HIGH |
| `redeclaration_same_scope` | var x; var x; (error?) | MEDIUM |

#### Control Flow Edge Cases
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `return_outside_function` | return 5; at top level (error) | HIGH |
| `empty_block` | { } | LOW |
| `return_in_init` | Initializer returns value (error) | MEDIUM |

#### Stack Depth
| Test Name | Description | Priority |
|-----------|-------------|----------|
| `deep_recursion` | Test stack overflow handling | LOW |
| `many_locals` | Function with 256+ locals | LOW |

---

### CATEGORY 10: OPTIMIZATION VERIFICATION (8 new tests)

| Test Name | Description | Priority |
|-----------|-------------|----------|
| `optimized_local_add` | Verify OP_AddLL optimization | MEDIUM |
| `optimized_local_subtract` | Verify OP_SubtractLL optimization | MEDIUM |
| `optimized_subtract_const` | Verify OP_SubtractLC/CL | MEDIUM |
| `long_jump_optimization` | Large code blocks use long jumps | LOW |
| `constant_folding` | Check if 1+1 optimized to 2 | LOW |
| `dead_code_elimination` | Unreachable code after return | LOW |
| `loop_optimization` | Loop optimizations applied | LOW |
| `constant_uniformity` | Constant pool optimization | LOW |

---

## SUMMARY BY PRIORITY

### HIGH Priority (68 tests)
- All basic operators and statements
- Variables, functions, classes
- Control flow basics
- Common error cases

### MEDIUM Priority (20 tests)
- Advanced features
- Nested constructs
- Scope complexity
- Edge cases

### LOW Priority (16 tests)
- Optimization verification
- Exotic edge cases
- Performance tests
- Deep recursion

---

## RECOMMENDED IMPLEMENTATION ORDER

### Phase 1: Core Language Features (32 tests)
1. Comparison operators (5)
2. Logical operators (3)
3. Unary operators (4)
4. Variables (8 HIGH priority)
5. Control flow (9 HIGH priority)
6. Strings (3 HIGH priority)

### Phase 2: Functions & Closures (11 tests)
7. Function basics (7)
8. Closures (3)
9. Recursion (1)

### Phase 3: OOP Features (13 tests)
10. Classes (4)
11. Instances (5)
12. Methods (4)

### Phase 4: Edge Cases & Hardening (12 tests)
13. Error cases (6 HIGH)
14. Numeric edge cases (3)
15. Expression complexity (3)

### Phase 5: Advanced & Optimization (20 tests)
16. First-class functions (3)
17. Advanced OOP (5)
18. Optimization tests (8)
19. Low-priority edge cases (4)

---

## TOTAL TEST COUNT

- Current tests: **11**
- New tests needed: **93**
- **Total: 104 comprehensive tests**

---

## TESTING METHODOLOGY

### For Each Test:
1. Create `.pg` script with `__dprint()` statements
2. Create `.expected` file with expected output
3. Add `TEST_F(ScriptTestBench, TestName) { testScript("test_name"); }` to testbench

### Expected Coverage:
- **100% opcode coverage** (all 54 opcodes)
- **100% statement coverage** (all 8 statement types)
- **100% operator coverage** (all operators)
- **100% error case coverage** (compile & runtime errors)

---

## NOTES

1. Some tests may reveal missing features or bugs
2. Optimization tests require inspecting bytecode output
3. Some error tests need `testScriptError()` helper
4. Priority levels can be adjusted based on project needs
5. Tests should be independent and deterministic
