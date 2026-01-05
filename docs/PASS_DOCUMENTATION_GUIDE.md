# Pass Documentation Guide

## Overview

This project uses automatic documentation generation for bytecode optimization passes. When you add or modify a pass, the documentation is automatically extracted and regenerated during CI.

## Quick Start

Add a documentation block to your pass header file:

```cpp
/**
 * @pass_doc
 * @name: My Awesome Pass
 * @purpose: Short description of what this pass does
 * @category: arithmetic
 * @example_before:
 *   OP_Something
 *   OP_Something_Else
 * @example_after:
 *   OP_Optimized
 * @benefits:
 *   - Faster execution
 *   - Smaller bytecode
 * @end_pass_doc
 */
```

## Documentation Tags

### Required Tags

- **@pass_doc** / **@end_pass_doc**: Marks the beginning and end of documentation
- **@name**: Human-readable name for the pass
- **@purpose**: One-sentence description
- **@category**: Category for organization (see categories below)

### Optional Tags

- **@example_before**: Code before optimization (multiline)
- **@example_after**: Code after optimization (multiline)
- **@benefits**: List of benefits (use `-` for bullet points, multiline)
- **@additional_notes**: Any extra information (multiline)

### Automatically Extracted

These fields are **automatically extracted from your C++ code** - do not include them in `@pass_doc`:

- **changes_size**: Extracted from `changesSize()` method return value
- **multi_pass**: Extracted from `requiresMultiplePasses()` method return value

This ensures documentation always matches the actual implementation!

## Categories

Use one of these standard categories:

- `arithmetic` - Math operations, constant folding
- `control_flow` - Jumps, loops, branches
- `stack` - Stack manipulation (push, pop)
- `memory` - Memory access, locals, globals
- `function_calls` - Function call optimization
- `general` - Doesn't fit other categories

## Examples

### Minimal Documentation

```cpp
/**
 * @pass_doc
 * @name: Dead Code Elimination
 * @purpose: Removes unreachable code after returns and unconditional jumps
 * @category: control_flow
 * @end_pass_doc
 */
```

### Full Documentation

```cpp
/**
 * @pass_doc
 * @name: Increment Optimization Pass
 * @purpose: Converts x = x + 1 patterns to specialized increment instructions
 * @category: arithmetic
 * @example_before:
 *   OP_Get_Local 0
 *   OP_Constant 1
 *   OP_Add
 *   OP_Set_Local 0
 *   OP_Pop
 * @example_after:
 *   OP_Incr_Local 0
 * @benefits:
 *   - Reduces instruction count from 5 to 1 (80% reduction)
 *   - Eliminates constant table access
 *   - Significantly faster in tight loops
 *   - Better instruction cache utilization
 * @changes_size: true
 * @multi_pass: false
 * @complexity: O(n) single pass over bytecode
 * @impact: High - for loops with incrementing counters benefit most
 * @additional_notes:
 *   This pass recognizes several patterns:
 *   - Pre-increment: ++x
 *   - Post-increment: x++
 *   - Works with both local and global variables
 *
 *   The pass validates that the same variable appears in both
 *   GET and SET instructions to avoid incorrect transformations.
 * @end_pass_doc
 */
```

## Fallback Behavior

If a pass file doesn't have a `@pass_doc` block, the script will:

1. Extract the class name
2. Extract `getName()` return value
3. Extract `changesSize()` and `requiresMultiplePasses()` return values
4. Generate basic documentation

This ensures all passes appear in the docs, but annotated passes will have much better documentation.

## Local Testing

Generate documentation locally:

```bash
# Basic generation
python scripts/generate_pass_docs.py

# With verbose output
python scripts/generate_pass_docs.py --verbose

# Custom paths
python scripts/generate_pass_docs.py \
  --passes-dir src/Engine/Compiler/pass \
  --output docs/source/script/my_passes.rst
```

## CI Integration

The documentation is automatically regenerated when:

- Any file in `src/Engine/Compiler/pass/` changes
- The generator script changes
- You manually trigger the workflow

The workflow will:

1. Extract documentation from all pass files
2. Generate `docs/source/script/passes_reference.rst`
3. Update `docs/source/index.rst` if needed
4. Commit changes back to the repository (with `[skip ci]` to avoid loops)

## Best Practices

1. **Write documentation first**: Add the `@pass_doc` block before implementing
2. **Keep examples realistic**: Use actual bytecode patterns from your VM
3. **Quantify benefits**: "40% faster" is better than "faster"
4. **Explain trade-offs**: If multi-pass has overhead, mention it
5. **Update when changing behavior**: Keep docs in sync with code

## Troubleshooting

### Documentation not updating

- Check that your `@pass_doc` block is in a `/**` comment (not `//`)
- Ensure `@end_pass_doc` is present
- Run locally with `--verbose` to see what was extracted

### Wrong category

- Use one of the standard categories listed above
- If you need a new category, update the generator script

### Examples not rendering correctly

- Ensure lines in `@example_before/after` don't start with `*`
- The script automatically strips leading `*` from multiline blocks

## Alternatives to This Approach

See the analysis in `docs/DOCUMENTATION_ALTERNATIVES.md` for other approaches we considered.
