#pragma once

#include "interpreter.h"

namespace pg
{
    class PrinterResolver : public Visitor
    {
    public:
        // Keep the base-class defaults visible for the node overloads this
        // visitor does not override
        using Visitor::visit;
        using Visitor::visitStatement;

        PrinterResolver() : Visitor() {}
        
        virtual std::shared_ptr<Valuable> visit(BinaryExpression *expr) override;
        virtual std::shared_ptr<Valuable> visit(LogicExpression *expr) override;
        virtual std::shared_ptr<Valuable> visit(UnaryExpression *expr) override;
        virtual std::shared_ptr<Valuable> visit(PreFixExpression *expr) override;
        virtual std::shared_ptr<Valuable> visit(PostFixExpression *expr) override;
        virtual std::shared_ptr<Valuable> visit(CompoundAtom *expr) override;
        virtual std::shared_ptr<Valuable> visit(Atom *expr) override;
        virtual std::shared_ptr<Valuable> visit(List *expr) override;
        virtual std::shared_ptr<Valuable> visit(This *expr) override;
        virtual std::shared_ptr<Valuable> visit(Var *expr) override;
        virtual std::shared_ptr<Valuable> visit(Assign *expr) override;
        virtual std::shared_ptr<Valuable> visit(CallExpression *expr) override;
        virtual std::shared_ptr<Valuable> visit(Get *expr) override;
        virtual std::shared_ptr<Valuable> visit(Set *expr) override;

        virtual void visitStatement(ExpressionStatement *stmt) override;
        virtual void visitStatement(VariableStatement *stmt) override;
        virtual void visitStatement(FunctionStatement *stmt) override;
        virtual void visitStatement(ClassStatement *stmt) override;
        virtual void visitStatement(BlockStatement *stmt) override;
        virtual void visitStatement(IfStatement *stmt) override;
        virtual void visitStatement(WhileStatement *stmt) override;
        virtual void visitStatement(ReturnStatement *stmt) override;
        virtual void visitStatement(ImportStatement *stmt) override;
    };
}