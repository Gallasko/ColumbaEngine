#pragma once

/**
 * @file ast_pass.h
 * @brief AST-level optimization pass infrastructure for the AST front-end.
 *
 * Counterpart of the bytecode PassManager (bytecode_pass.h), but running on
 * the parsed AST between Parser::parse and AstCompiler emission - the whole
 * point of the AST front-end. Passes here see program STRUCTURE (loops,
 * access chains) that the single-pass Pratt compiler and the bytecode
 * peepholes cannot recover.
 *
 * Passes transform the statement list in place (or annotate for the
 * lowering layer). They run ONLY on the AST front-end path; the Pratt
 * front-end is unaffected, which is exactly what the differential testbench
 * measures: same observable behavior, different code.
 *
 * Scope rules for pass authors:
 * - Do not duplicate bytecode-layer work (constant folding, jump shaping,
 *   operand elision live in Compiler/pass/ and decoded_fusion.h).
 * - Transformed ASTs are only ever emitted by AstCompiler; the desugared
 *   forms carried by For/ForIn/Index nodes are NOT kept in sync and must
 *   not be relied upon after a transforming pass ran.
 */

#include "Interpreter/statement.h"

#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace pg
{
    struct VM;

    class AstPass
    {
    public:
        virtual ~AstPass() = default;

        virtual std::string getName() const = 0;

        /** Transform/annotate the program; return true if anything changed */
        virtual bool runPass(VM* vm, std::queue<StatementPtr>& statements) = 0;
    };

    class AstPassManager
    {
    public:
        void addPass(std::unique_ptr<AstPass> pass);

        void runAllPasses(VM* vm, std::queue<StatementPtr>& statements);

        void setDebugOutput(bool enable) { enableDebugOutput = enable; }

        void listPasses() const;

        void clearPasses();

        size_t getPassCount() const { return passes.size(); }

    private:
        void debugPrint(const std::string& message) const;

        std::vector<std::unique_ptr<AstPass>> passes;
        bool enableDebugOutput = false;
    };
}
