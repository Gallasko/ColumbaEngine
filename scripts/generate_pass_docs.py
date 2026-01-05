#!/usr/bin/env python3
"""
Automatic Bytecode Pass Documentation Generator

This script scans all bytecode optimization passes in src/Engine/Compiler/pass/
and extracts structured documentation from special comment blocks to generate
RST documentation for ReadTheDocs.

Documentation Format:
    /**
     * @pass_doc
     * @name: Pass Name
     * @purpose: Brief description
     * @category: optimization_type (e.g., arithmetic, control_flow, stack)
     * @example_before:
     *   OP_Get_Local 0
     *   OP_Constant 1
     *   OP_Add
     * @example_after:
     *   OP_Incr_Local 0
     * @benefits:
     *   - Reduces instruction count from 3 to 1
     *   - Faster execution in loops
     * @additional_notes: Any extra context or details (optional)
     * @end_pass_doc
     */

Note: @changes_size and @multi_pass are automatically extracted from the
      changesSize() and requiresMultiplePasses() methods in the code.

Usage:
    python scripts/generate_pass_docs.py
    python scripts/generate_pass_docs.py --output docs/source/script/passes_reference.rst
"""

import re
import sys
from pathlib import Path
from typing import List, Optional
from dataclasses import dataclass, field
from collections import defaultdict


@dataclass
class PassDocumentation:
    """Stores extracted documentation for a single optimization pass."""
    name: str = ""
    class_name: str = ""
    purpose: str = ""
    category: str = "general"
    example_before: List[str] = field(default_factory=list)
    example_after: List[str] = field(default_factory=list)
    benefits: List[str] = field(default_factory=list)
    changes_size: bool = False
    multi_pass: bool = False
    complexity: str = ""
    impact: str = ""
    file_path: str = ""
    additional_notes: str = ""


class PassDocExtractor:
    """Extracts documentation from C++ pass files."""

    def __init__(self, passes_dir: str):
        self.passes_dir = Path(passes_dir)
        self.passes: List[PassDocumentation] = []

    def extract_all_passes(self) -> List[PassDocumentation]:
        """Scan all .h and .cpp files in the passes directory."""
        for file_path in self.passes_dir.glob("*.h"):
            doc = self._extract_from_file(file_path)
            if doc:
                self.passes.append(doc)

        for file_path in self.passes_dir.glob("*.cpp"):
            # Only process .cpp if no corresponding .h was found
            h_file = file_path.with_suffix('.h')
            if not h_file.exists():
                doc = self._extract_from_file(file_path)
                if doc:
                    self.passes.append(doc)

        return self.passes

    def _extract_from_file(self, file_path: Path) -> Optional[PassDocumentation]:
        """Extract documentation from a single file."""
        try:
            content = file_path.read_text()
        except Exception as e:
            print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
            return None

        # Look for @pass_doc block
        doc_pattern = r'/\*\*.*?@pass_doc(.*?)@end_pass_doc.*?\*/'
        match = re.search(doc_pattern, content, re.DOTALL)

        if not match:
            # If no @pass_doc block, try to extract basic info from class
            return self._extract_basic_info(file_path, content)

        doc_block = match.group(1)
        doc = PassDocumentation()
        doc.file_path = str(file_path.relative_to(self.passes_dir.parent.parent.parent))

        # Extract class name
        class_match = re.search(r'class\s+(\w+)\s*:\s*public\s+BytecodePass', content)
        if class_match:
            doc.class_name = class_match.group(1)

        # Parse documentation fields
        self._parse_field(doc_block, r'@name:\s*(.+)', lambda v: setattr(doc, 'name', v.strip()))
        self._parse_field(doc_block, r'@purpose:\s*(.+)', lambda v: setattr(doc, 'purpose', v.strip()))
        self._parse_field(doc_block, r'@category:\s*(\w+)', lambda v: setattr(doc, 'category', v.strip()))
        self._parse_field(doc_block, r'@additional_notes:\s*(.+?)(?=@|\Z)',
                         lambda v: setattr(doc, 'additional_notes', v.strip()), re.DOTALL)

        # Extract boolean fields from C++ code (not from doc comments)
        # This ensures they're always in sync with the actual implementation
        doc.changes_size = self._extract_bool_from_code(content, 'changesSize')
        doc.multi_pass = self._extract_bool_from_code(content, 'requiresMultiplePasses')

        # Parse example_before (multiline)
        before_match = re.search(r'@example_before:(.*?)(?=@example_after|@)', doc_block, re.DOTALL)
        if before_match:
            lines = []
            for line in before_match.group(1).strip().split('\n'):
                # Strip whitespace and leading *
                cleaned = line.strip()
                if cleaned.startswith('*'):
                    cleaned = cleaned[1:].strip()
                if cleaned:  # Only add non-empty lines
                    lines.append(cleaned)
            doc.example_before = lines

        # Parse example_after (multiline)
        after_match = re.search(r'@example_after:(.*?)(?=@|\Z)', doc_block, re.DOTALL)
        if after_match:
            lines = []
            for line in after_match.group(1).strip().split('\n'):
                # Strip whitespace and leading *
                cleaned = line.strip()
                if cleaned.startswith('*'):
                    cleaned = cleaned[1:].strip()
                if cleaned:  # Only add non-empty lines
                    lines.append(cleaned)
            doc.example_after = lines

        # Parse benefits (multiline list)
        benefits_match = re.search(r'@benefits:(.*?)(?=@|\Z)', doc_block, re.DOTALL)
        if benefits_match:
            benefits = []
            for line in benefits_match.group(1).split('\n'):
                # Strip whitespace and leading *
                cleaned = line.strip()
                if cleaned.startswith('*'):
                    cleaned = cleaned[1:].strip()
                # Look for lines starting with -
                if cleaned.startswith('-'):
                    benefits.append(cleaned[1:].strip())
            doc.benefits = benefits

        return doc if doc.name else None

    def _extract_basic_info(self, file_path: Path, content: str) -> Optional[PassDocumentation]:
        """Extract basic info from a pass without @pass_doc block."""
        class_match = re.search(r'class\s+(\w+)\s*:\s*public\s+BytecodePass', content)
        if not class_match:
            return None

        doc = PassDocumentation()
        doc.class_name = class_match.group(1)
        doc.file_path = str(file_path.relative_to(self.passes_dir.parent.parent.parent))

        # Try to extract getName()
        name_match = re.search(r'getName\(\).*?return\s+"([^"]+)"', content, re.DOTALL)
        if name_match:
            doc.name = name_match.group(1)
        else:
            doc.name = doc.class_name

        # Extract booleans from code
        changes_match = re.search(r'changesSize\(\).*?return\s+(true|false)', content, re.DOTALL)
        if changes_match:
            doc.changes_size = changes_match.group(1) == 'true'

        multi_match = re.search(r'requiresMultiplePasses\(\).*?return\s+(true|false)', content, re.DOTALL)
        if multi_match:
            doc.multi_pass = multi_match.group(1) == 'true'

        doc.purpose = f"Auto-extracted pass: {doc.name}"
        return doc

    def _parse_field(self, text: str, pattern: str, callback, flags=0):
        """Parse a single-line field and call callback with the value."""
        match = re.search(pattern, text, flags)
        if match:
            callback(match.group(1))

    def _parse_bool(self, text: str, pattern: str) -> bool:
        """Parse a boolean field."""
        match = re.search(pattern, text)
        return match.group(1) == 'true' if match else False

    def _extract_bool_from_code(self, content: str, method_name: str) -> bool:
        """Extract boolean return value from C++ method implementation.

        Looks for patterns like:
            bool changesSize() const override { return true; }
            bool requiresMultiplePasses() const override { return false; }
        """
        pattern = rf'{method_name}\s*\(\).*?return\s+(true|false)'
        match = re.search(pattern, content, re.DOTALL)
        return match.group(1) == 'true' if match else False


class RSTGenerator:
    """Generates RST documentation from extracted pass data."""

    def __init__(self, passes: List[PassDocumentation], repo_url: str):
        self.passes = passes
        self.repo_url = repo_url
        self.categories = defaultdict(list)
        self._categorize_passes()

    def _categorize_passes(self):
        """Group passes by category."""
        for pass_doc in self.passes:
            self.categories[pass_doc.category].append(pass_doc)

    def generate(self) -> str:
        """Generate complete RST document."""
        rst = []
        rst.append("Bytecode Optimization Passes Reference")
        rst.append("=" * 50)
        rst.append("")
        rst.append(".. note::")
        rst.append("   This document is automatically generated from pass source code.")
        rst.append("   Last updated: Auto-generated during build.")
        rst.append("")
        rst.append("This reference documents all bytecode optimization passes available in the VM.")
        rst.append("")

        # Table of contents
        rst.append(".. contents:: Table of Contents")
        rst.append("   :local:")
        rst.append("   :depth: 2")
        rst.append("")

        # Overview section
        rst.append("Overview")
        rst.append("-" * 8)
        rst.append("")
        rst.append(f"The VM includes **{len(self.passes)} optimization passes** organized into categories:")
        rst.append("")
        for category, passes in sorted(self.categories.items()):
            rst.append(f"* **{category.replace('_', ' ').title()}**: {len(passes)} passes")
        rst.append("")

        # Summary table
        rst.append("Quick Reference")
        rst.append("-" * 15)
        rst.append("")
        rst.append(".. list-table::")
        rst.append("   :header-rows: 1")
        rst.append("   :widths: 25 40 15 10 10")
        rst.append("")
        rst.append("   * - Pass Name")
        rst.append("     - Purpose")
        rst.append("     - Category")
        rst.append("     - Size Change")
        rst.append("     - Multi-Pass")

        for pass_doc in sorted(self.passes, key=lambda p: p.name):
            rst.append(f"   * - :ref:`{pass_doc.name} <{self._make_anchor(pass_doc.name)}>`")
            purpose_short = pass_doc.purpose[:60] + "..." if len(pass_doc.purpose) > 60 else pass_doc.purpose
            rst.append(f"     - {purpose_short}")
            rst.append(f"     - {pass_doc.category.replace('_', ' ').title()}")
            rst.append(f"     - {'Yes' if pass_doc.changes_size else 'No'}")
            rst.append(f"     - {'Yes' if pass_doc.multi_pass else 'No'}")

        rst.append("")

        # Detailed documentation by category
        for category, passes in sorted(self.categories.items()):
            rst.extend(self._generate_category_section(category, passes))

        # Footer
        rst.append("")
        rst.append("Source Code")
        rst.append("-" * 11)
        rst.append("")
        rst.append(f"All optimization passes are located in `src/Engine/Compiler/pass/ <{self.repo_url}/tree/main/src/Engine/Compiler/pass>`_")
        rst.append("")

        return "\n".join(rst)

    def _generate_category_section(self, category: str, passes: List[PassDocumentation]) -> List[str]:
        """Generate RST for a category of passes."""
        rst = []
        category_title = category.replace('_', ' ').title()
        rst.append("")
        rst.append(category_title)
        rst.append("~" * len(category_title))
        rst.append("")

        for pass_doc in sorted(passes, key=lambda p: p.name):
            rst.extend(self._generate_pass_section(pass_doc))

        return rst

    def _generate_pass_section(self, pass_doc: PassDocumentation) -> List[str]:
        """Generate RST for a single pass."""
        rst = []

        # Pass title with anchor
        rst.append("")
        rst.append(f".. _{self._make_anchor(pass_doc.name)}:")
        rst.append("")
        rst.append(pass_doc.name)
        rst.append("^" * len(pass_doc.name))
        rst.append("")

        # Purpose
        if pass_doc.purpose:
            rst.append(f"**Purpose**: {pass_doc.purpose}")
            rst.append("")

        # Properties
        rst.append("**Properties**:")
        rst.append("")
        rst.append(f"* Class name: ``{pass_doc.class_name}``")
        rst.append(f"* Changes bytecode size: {'Yes' if pass_doc.changes_size else 'No'}")
        rst.append(f"* Requires multiple passes: {'Yes' if pass_doc.multi_pass else 'No'}")
        rst.append("")

        # Example transformation
        if pass_doc.example_before and pass_doc.example_after:
            rst.append("**Transformation Example**")
            rst.append("")
            rst.append("Before::")
            rst.append("")
            for line in pass_doc.example_before:
                rst.append(f"    {line}")
            rst.append("")
            rst.append("After::")
            rst.append("")
            for line in pass_doc.example_after:
                rst.append(f"    {line}")
            rst.append("")

        # Benefits
        if pass_doc.benefits:
            rst.append("**Benefits**:")
            rst.append("")
            for benefit in pass_doc.benefits:
                rst.append(f"* {benefit}")
            rst.append("")

        # Additional notes
        if pass_doc.additional_notes:
            rst.append("**Notes**")
            rst.append("")
            # Process additional notes to handle multiline properly
            # Remove leading asterisks and extra whitespace from comment blocks
            notes = pass_doc.additional_notes.strip()
            # Split into lines and clean each line
            lines = notes.split('\n')
            cleaned_lines = []
            for line in lines:
                # Remove leading * and whitespace
                cleaned = line.strip()
                if cleaned.startswith('*'):
                    cleaned = cleaned[1:].strip()
                if cleaned:
                    cleaned_lines.append(cleaned)

            # Join lines and add as paragraph
            if cleaned_lines:
                rst.append(' '.join(cleaned_lines))
                rst.append("")

        # Source link
        source_url = f"{self.repo_url}/blob/main/{pass_doc.file_path}"
        rst.append(f"**Source**: `{Path(pass_doc.file_path).name} <{source_url}>`_")
        rst.append("")

        return rst

    def _make_anchor(self, name: str) -> str:
        """Create a valid RST anchor from pass name."""
        return name.lower().replace(' ', '-').replace('_', '-')


def main():
    """Main entry point."""
    import argparse

    parser = argparse.ArgumentParser(description="Generate bytecode pass documentation")
    parser.add_argument('--passes-dir',
                       default='src/Engine/Compiler/pass',
                       help='Directory containing pass files')
    parser.add_argument('--output',
                       default='docs/source/script/passes_reference.rst',
                       help='Output RST file')
    parser.add_argument('--repo-url',
                       default='https://github.com/Gallasko/ColumbaEngine',
                       help='GitHub repository URL')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')

    args = parser.parse_args()

    # Extract documentation
    if args.verbose:
        print(f"Scanning passes in: {args.passes_dir}")

    extractor = PassDocExtractor(args.passes_dir)
    passes = extractor.extract_all_passes()

    if args.verbose:
        print(f"Found {len(passes)} passes:")
        for p in passes:
            status = "✓ documented" if p.purpose != f"Auto-extracted pass: {p.name}" else "⚠ auto-extracted"
            print(f"  - {p.name} ({status})")

    # Generate RST
    generator = RSTGenerator(passes, args.repo_url)
    rst_content = generator.generate()

    # Write output
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(rst_content)

    print(f"✓ Generated documentation: {args.output}")
    print(f"  Total passes: {len(passes)}")
    documented = sum(1 for p in passes if p.purpose != f"Auto-extracted pass: {p.name}")
    print(f"  Fully documented: {documented}/{len(passes)}")

    if documented < len(passes):
        print("\n⚠ Some passes are missing @pass_doc blocks")
        print("  Add documentation blocks to improve the generated docs")

    return 0


if __name__ == '__main__':
    sys.exit(main())
