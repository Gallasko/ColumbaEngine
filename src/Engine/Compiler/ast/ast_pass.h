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
#include <set>
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

        // A module is compiled below the script that imports it, with what the passes learnt from
        // that script's whole program. These two say what of it a module's compile can see, so a
        // module compiled once can be kept for the next program that tells it the same
        // (modulecache)

        /** The names a module can be given besides its own: an inlined body brings the names it reads */
        virtual void namesBrought(std::set<std::string>& names) const { (void)names; }

        /**
         * What the pass learnt about these names, as text: a module that uses only these names
         * comes out the same under two programs with the same text. A node named by its address
         * goes in `keep`, so that address is not given to another while the text is kept
         */
        virtual std::string factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const
        {
            (void)names;
            (void)keep;
            return "";
        }
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

        /** The passes, by name and in order */
        std::string passNames() const;

        /** What every pass learnt about the names a module uses, and those they bring (AstPass::factsAbout) */
        std::string moduleFacts(std::set<std::string> names, std::vector<std::shared_ptr<const void>>& keep) const;

    private:
        void debugPrint(const std::string& message) const;

        std::vector<std::unique_ptr<AstPass>> passes;
        bool enableDebugOutput = false;
    };
}
