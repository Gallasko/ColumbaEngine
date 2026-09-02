#pragma once

/**
 * @file ast_compiler.h
 * @brief AST -> bytecode front-end for PgScript.
 *
 * Second compiler front-end: where the Pratt front-end (CParser) emits
 * bytecode in a single pass over the token stream, this one first builds the
 * AST (Interpreter/parser.h) and then walks it as a Visitor, emitting into
 * the exact same Chunk/ObjFunction representation. Having the AST available
 * is what allows whole-program optimization passes (entity-loop and
 * component-access lowering) to run before emission.
 *
 * Emission reuses the existing machinery wholesale:
 * - The Compiler struct provides scopes, locals, upvalues, loop contexts and
 *   the nested-compiler chain (Compiler::current).
 * - The root Compiler's embedded CParser is used purely as the emission
 *   backend (writeByte/writeConstant/emitJump/patchJump/emitLoop/emitReturn
 *   and parseImportFile); it never consumes tokens on this path. All those
 *   helpers emit through the static Compiler::current, so nested function
 *   compilation works exactly like on the Pratt path.
 *
 * The bytecode produced must stay semantically identical to CParser output;
 * the differential testbench compares both front-ends over the script corpus.
 */

#include "Interpreter/interpreter.h"
#include "Interpreter/token.h"

#include "../compiler.h"

#include <queue>

namespace pg
{
    struct VM;

    class AstCompiler : public Visitor
    {
    public:
        AstCompiler(VM* vm) : vm(vm), root(vm) {}

        /**
         * Full front-end entry point: tokens -> AST -> bytecode.
         *
         * Returns the top-level script function Value, or 0x0 on error.
         */
        Value compile(std::queue<Token> tokens);

        /** Emit bytecode from an already parsed AST */
        Value compileAst(std::queue<StatementPtr> statements);

        bool hasError() const { return hadError or root.parser.hasError(); }

        /** Compiled function values owned by this front-end (root script + nested functions/imports) */
        std::vector<Value>& allocatedFunctions() { return root.parser.allocatedFunction; }

        // Expressions
        virtual std::shared_ptr<Valuable> visit(BinaryExpression* expr) override;
        virtual std::shared_ptr<Valuable> visit(LogicExpression* expr) override;
        virtual std::shared_ptr<Valuable> visit(UnaryExpression* expr) override;
        virtual std::shared_ptr<Valuable> visit(PreFixExpression* expr) override;
        virtual std::shared_ptr<Valuable> visit(PostFixExpression* expr) override;
        virtual std::shared_ptr<Valuable> visit(CompoundAtom* expr) override;
        virtual std::shared_ptr<Valuable> visit(Atom* expr) override;
        virtual std::shared_ptr<Valuable> visit(List* expr) override;
        virtual std::shared_ptr<Valuable> visit(This* expr) override;
        virtual std::shared_ptr<Valuable> visit(Var* expr) override;
        virtual std::shared_ptr<Valuable> visit(Assign* expr) override;
        virtual std::shared_ptr<Valuable> visit(CallExpression* expr) override;
        virtual std::shared_ptr<Valuable> visit(Get* expr) override;
        virtual std::shared_ptr<Valuable> visit(Set* expr) override;
        virtual std::shared_ptr<Valuable> visit(AnonymousFunction* expr) override;
        virtual std::shared_ptr<Valuable> visit(IndexGet* expr) override;
        virtual std::shared_ptr<Valuable> visit(IndexSet* expr) override;

        // Statements
        virtual void visitStatement(ExpressionStatement* stmt) override;
        virtual void visitStatement(VariableStatement* stmt) override;
        virtual void visitStatement(FunctionStatement* stmt) override;
        virtual void visitStatement(ClassStatement* stmt) override;
        virtual void visitStatement(BlockStatement* stmt) override;
        virtual void visitStatement(IfStatement* stmt) override;
        virtual void visitStatement(WhileStatement* stmt) override;
        virtual void visitStatement(ReturnStatement* stmt) override;
        virtual void visitStatement(ImportStatement* stmt) override;
        virtual void visitStatement(ForStatement* stmt) override;
        virtual void visitStatement(ForInStatement* stmt) override;
        virtual void visitStatement(BreakStatement* stmt) override;
        virtual void visitStatement(ContinueStatement* stmt) override;
        virtual void visitStatement(DPrintStatement* stmt) override;

    private:
        /** The emission backend: the root compiler's embedded CParser */
        CParser& emitter() { return root.parser; }

        /** Set the line reported for subsequently emitted bytecode */
        void setLine(const Token& token) { emitter().previousToken = token; }

        void error(const Token& token, const std::string& message);

        /**
         * Intern a property/method name into the current chunk's
         * constantStrings, mirroring the Pratt front-end's dot() helper.
         * Returns -1 on overflow (and reports the error).
         */
        int internString(const std::string& name, const Token& token);

        /** Emit a value read/write for a named variable (local/upvalue/global) */
        void emitVariableGet(const Token& name);

        /**
         * Compile a nested function (named, method or anonymous) exactly like
         * CParser::parseFunction: nested Compiler, params as locals, body,
         * OP_Closure + upvalue operands.
         */
        void compileFunction(FunctionType type, const std::string& name, std::queue<ExprPtr> parameters, StatementPtr body, const Token& token);

        /** Emit the entry cleanup shared by break/continue (pop locals deeper than the loop) */
        void emitLoopScopeUnwind();

        VM* vm;

        /** Root compiler for the top-level script; its parser is the emission backend */
        Compiler root;

        bool hadError = false;
    };
}
