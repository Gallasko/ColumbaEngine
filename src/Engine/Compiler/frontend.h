#pragma once

#include <cstdint>

namespace pg
{
    /**
     * Compiler front-end selection for PgScript.
     *
     * Pratt: the single-pass token->bytecode compiler (CParser).
     * Ast:   the AST front-end (AstCompiler) - parses to an AST first so
     *        whole-program optimization passes can run before emission.
     *
     * Both emit the same bytecode representation and share the
     * pass/decode/run pipeline; kept selectable until benchmarks decide a
     * winner.
     */
    enum class ScriptFrontEnd : uint8_t
    {
        Pratt,
        Ast
    };
}
