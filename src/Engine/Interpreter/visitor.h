#pragma once

#include <stdexcept>
#include <string>

#include "expression.h"
#include "statement.h"

namespace pg
{
    /**
     * @brief Visitor over the PgScript AST built by the Parser
     *
     * The nodes that have a desugared form (index access, for loops) run it by default,
     * only the visitors that care about the structured form override them.
     */
    class Visitor
    {
    public:
        virtual ~Visitor() {}

        virtual void visit(BinaryExpression *expr) = 0;
        virtual void visit(LogicExpression *expr) = 0;
        virtual void visit(UnaryExpression *expr) = 0;
        virtual void visit(PreFixExpression *expr) = 0;
        virtual void visit(PostFixExpression *expr) = 0;
        virtual void visit(CompoundAtom *expr) = 0;
        virtual void visit(Atom *expr) = 0;
        virtual void visit(NoneAtom *expr) = 0;
        virtual void visit(List *expr) = 0;
        virtual void visit(This *expr) = 0;
        virtual void visit(Var *expr) = 0;
        virtual void visit(Assign *expr) = 0;
        virtual void visit(CallExpression *expr) = 0;
        virtual void visit(Get *expr) = 0;
        virtual void visit(Set *expr) = 0;

        virtual void visit(AnonymousFunction *expr)
        {
            throw std::runtime_error("Anonymous functions are not supported by this visitor (line " + std::to_string(expr->token.line) + ")");
        }

        virtual void visit(IndexGet *expr)
        {
            if (not expr->desugared)
                throw std::runtime_error("IndexGet without desugared form");

            expr->desugared->accept(this);
        }

        virtual void visit(IndexSet *expr)
        {
            if (not expr->desugared)
                throw std::runtime_error("IndexSet without desugared form");

            expr->desugared->accept(this);
        }

        virtual void visitStatement(ExpressionStatement *stmt) = 0;
        virtual void visitStatement(VariableStatement *stmt) = 0;
        virtual void visitStatement(FunctionStatement *stmt) = 0;
        virtual void visitStatement(ClassStatement *stmt) = 0;
        virtual void visitStatement(BlockStatement *stmt) = 0;
        virtual void visitStatement(IfStatement *stmt) = 0;
        virtual void visitStatement(WhileStatement *stmt) = 0;
        virtual void visitStatement(ReturnStatement *stmt) = 0;
        virtual void visitStatement(ImportStatement *stmt) = 0;

        virtual void visitStatement(ForStatement *stmt)
        {
            if (not stmt->desugared)
                throw std::runtime_error("ForStatement without desugared form");

            stmt->desugared->accept(this);
        }

        virtual void visitStatement(ForInStatement *stmt)
        {
            if (not stmt->desugared)
                throw std::runtime_error("ForInStatement without desugared form");

            stmt->desugared->accept(this);
        }

        virtual void visitStatement(BreakStatement *stmt)
        {
            throw std::runtime_error("'break' is not supported by this visitor (line " + std::to_string(stmt->token.line) + ")");
        }

        virtual void visitStatement(ContinueStatement *stmt)
        {
            throw std::runtime_error("'continue' is not supported by this visitor (line " + std::to_string(stmt->token.line) + ")");
        }

        virtual void visitStatement(DPrintStatement *stmt)
        {
            throw std::runtime_error("'__dprint' is not supported by this visitor (line " + std::to_string(stmt->token.line) + ")");
        }
    };
}
