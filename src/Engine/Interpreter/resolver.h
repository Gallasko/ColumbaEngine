#pragma once

#include <unordered_map>
#include <stack>

#include "interpreter.h"

namespace pg
{

    class ScopeStack : public std::stack<std::unordered_map<std::string, bool>>
    {
    public:
        const std::unordered_map<std::string, bool>& at(unsigned int index) const { return c.at(index); }
    };

    // Forward declaration
    class Valuable;

    class VisitorResolver : public Visitor
    {
        enum class FunctionType
        {
            NONE,
            FUNCTION,
            METHOD
        };

        enum class ClassType
        {
            NONE,
            CLASS
        };

    public:
        // Keep the base-class defaults visible for the node overloads this
        // visitor does not override (For/ForIn/Index resolve through their
        // desugared form via the Visitor defaults)
        using Visitor::visit;
        using Visitor::visitStatement;

        VisitorResolver() : Visitor() {}
        
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
        virtual std::shared_ptr<Valuable> visit(AnonymousFunction *expr) override;
        // IndexGet / IndexSet use the Visitor base default (resolve through the
        // desugared 'at'/'set' call chain; children are shared with the
        // structured form so both stay resolved)

        virtual void visitStatement(ExpressionStatement *stmt) override;
        virtual void visitStatement(VariableStatement *stmt) override;
        virtual void visitStatement(FunctionStatement *stmt) override;
        virtual void visitStatement(ClassStatement *stmt) override;
        virtual void visitStatement(BlockStatement *stmt) override;
        virtual void visitStatement(IfStatement *stmt) override;
        virtual void visitStatement(WhileStatement *stmt) override;
        virtual void visitStatement(ReturnStatement *stmt) override;
        virtual void visitStatement(ImportStatement *stmt) override;
        // ForStatement / ForInStatement use the Visitor base default (resolve
        // through the desugared while-loop form)
        virtual void visitStatement(BreakStatement *stmt) override;
        virtual void visitStatement(ContinueStatement *stmt) override;
        virtual void visitStatement(DPrintStatement *stmt) override;

        inline const std::unordered_map<Expression*, unsigned int>& getLocals() const noexcept { return locals; }

    private:
        ScopeStack scopes;
        std::unordered_map<Expression*, unsigned int> locals;
        FunctionType currentFunction = FunctionType::NONE;
        ClassType currentClass = ClassType::NONE;

        void declare(const std::string& name);
        void define(const std::string& name);
        void resolveLocal(Expression* expression, const std::string& name);
        void resolveFunction(FunctionStatement* statement, const FunctionType& type);
    };

    class Resolver
    {
    public:
        Resolver(const std::queue<StatementPtr>& statements) : statements(statements), rVisitor() {}

        const std::unordered_map<Expression*, unsigned int>& resolve();
        const std::queue<StatementPtr>& getStatementsList() const { return statements; }

        inline bool hasError() const { return errorEncountered; }

    private:
        std::queue<StatementPtr> statements;
        VisitorResolver rVisitor;

        bool errorEncountered = false;
    };

}
