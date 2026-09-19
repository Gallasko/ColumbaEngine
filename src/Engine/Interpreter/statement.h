#pragma once

#include <string>
#include <queue>

#include "expression.h"

namespace pg
{

    class Statement
    {
    public:
        Statement() {}
        virtual ~Statement() {}

        virtual void accept(Visitor* visitor) = 0;
        virtual std::string prettyPrint() const = 0;
        virtual std::string getType() const = 0;
    };

    typedef std::shared_ptr<Statement> StatementPtr;

    struct ExpressionStatement : public Statement
    {
        ExpressionStatement(ExprPtr expression) : Statement(), expr(expression) {}
        ~ExpressionStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Expression statement: " + expr->prettyPrint(); }
        virtual std::string getType() const override { return "ExpressionStatement"; }

        ExprPtr expr;
    };

    struct VariableStatement : public Statement
    {
        VariableStatement(const Token& name, ExprPtr expression) : Statement(), name(name), expr(expression) {}
        ~VariableStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { auto initializerValue = expr != nullptr ? expr->prettyPrint() : "null"; return "Assignment statement, set variable :" + name.text + ", to value: " + initializerValue; }
        virtual std::string getType() const override { return "VariableStatement"; }

        Token name;
        ExprPtr expr;
    };

    struct FunctionStatement : public Statement
    {
        FunctionStatement(const Token& name, const std::queue<ExprPtr>& parameters, StatementPtr body) : Statement(), name(name), parameters(parameters), body(body) {}
        ~FunctionStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override {auto p = parameters; std::string res = ""; while(p.size() > 0) { res += p.front()->prettyPrint() + ", "; p.pop();} return "Function statement :" + name.text + " with parameters: " + res; }
        virtual std::string getType() const override { return "FunctionStatement"; }

        Token name;
        std::queue<ExprPtr> parameters;
        StatementPtr body;
    };

    struct ClassStatement : public Statement
    {
        ClassStatement(const Token& name, const std::queue<std::shared_ptr<FunctionStatement>>& methods) : Statement(), name(name), methods(methods) {}
        ~ClassStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override {auto p = methods; std::string res = ""; while(p.size() > 0) { res += p.front()->prettyPrint() + "\n"; p.pop();} return "Class statement :" + name.text + " with methodes: " + "\n" + res; }
 
        virtual std::string getType() const override { return "ClassStatement"; }

        Token name;
        std::queue<std::shared_ptr<FunctionStatement>> methods;
    };

    struct BlockStatement : public Statement
    {
        BlockStatement(const std::queue<StatementPtr>& statements) : Statement(), statements(statements) {}
        ~BlockStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override {auto p = statements; std::string res = ""; while(p.size() > 0) { res += p.front()->prettyPrint() + "\n"; p.pop();} return "Block Statement \n" + res; }
        virtual std::string getType() const override { return "BlockStatement"; }

        std::queue<StatementPtr> statements;
    };

    struct IfStatement : public Statement
    {
        IfStatement(ExprPtr condition, StatementPtr thenBranch, StatementPtr elseBranch) : Statement(), condition(condition), thenBranch(thenBranch), elseBranch(elseBranch) {}
        ~IfStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "If statement, with condition: " + condition->prettyPrint() + "\n" + thenBranch->prettyPrint() + "\n" + elseBranch->prettyPrint(); } 
        virtual std::string getType() const override { return "IfStatement"; }

        ExprPtr condition;
        StatementPtr thenBranch;
        StatementPtr elseBranch;
    };

    struct WhileStatement : public Statement
    {
        WhileStatement(ExprPtr condition, StatementPtr body) : Statement(), condition(condition), body(body) {}
        ~WhileStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "while statement, with condition: " + condition->prettyPrint() + "\n" + body->prettyPrint(); }
        virtual std::string getType() const override { return "WhileStatement"; }

        ExprPtr condition;
        StatementPtr body;
    };

    struct ReturnStatement : public Statement
    {
        ReturnStatement(const Token& name, ExprPtr value) : Statement(), name(name), value(value) {}
        ~ReturnStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Return statement with value: " + (value ? value->prettyPrint() : "null"); }
        virtual std::string getType() const override { return "ReturnStatement"; }

        Token name;
        ExprPtr value;
    };

    /**
     * C-style for loop: for (init; condition; increment) body
     *
     * The structured fields are used by the bytecode front-end and its
     * optimization passes; 'desugared' holds the equivalent while-loop form
     * executed by the tree-walking interpreter and resolved by the resolver
     * (children are shared pointers between both forms).
     */
    struct ForStatement : public Statement
    {
        ForStatement(StatementPtr initializer, ExprPtr condition, ExprPtr increment, StatementPtr body) : Statement(), initializer(initializer), condition(condition), increment(increment), body(body) {}
        ~ForStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "For statement with condition: " + (condition ? condition->prettyPrint() : "true") + "\n" + body->prettyPrint(); }
        virtual std::string getType() const override { return "ForStatement"; }

        StatementPtr initializer;
        ExprPtr condition;
        ExprPtr increment;
        StatementPtr body;

        /** Equivalent while-loop form for the tree-walking path */
        StatementPtr desugared;
    };

    /**
     * Range-based for loop: for (var name : iterable) body
     *
     * Same structured/desugared split as ForStatement; 'desugared' holds the
     * iterator-protocol while-loop (.it()/.begin()/.current()/.next()/.end())
     * the tree-walking interpreter executes.
     */
    struct ForInStatement : public Statement
    {
        ForInStatement(const Token& varName, StatementPtr initializer, ExprPtr iterable, StatementPtr body) : Statement(), varName(varName), initializer(initializer), iterable(iterable), body(body) {}
        ~ForInStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "ForIn statement over: " + iterable->prettyPrint() + "\n" + body->prettyPrint(); }
        virtual std::string getType() const override { return "ForInStatement"; }

        Token varName;
        StatementPtr initializer;
        ExprPtr iterable;
        StatementPtr body;

        /** Equivalent iterator-protocol while-loop for the tree-walking path */
        StatementPtr desugared;
    };

    struct BreakStatement : public Statement
    {
        BreakStatement(const Token& token) : Statement(), token(token) {}
        ~BreakStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Break statement"; }
        virtual std::string getType() const override { return "BreakStatement"; }

        Token token;
    };

    struct ContinueStatement : public Statement
    {
        ContinueStatement(const Token& token) : Statement(), token(token) {}
        ~ContinueStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Continue statement"; }
        virtual std::string getType() const override { return "ContinueStatement"; }

        Token token;
    };

    /** __dprint(expr) debug-output statement, used by the script test benches */
    struct DPrintStatement : public Statement
    {
        DPrintStatement(const Token& token, ExprPtr expr) : Statement(), token(token), expr(expr) {}
        ~DPrintStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "DPrint statement: " + expr->prettyPrint(); }
        virtual std::string getType() const override { return "DPrintStatement"; }

        Token token;
        ExprPtr expr;
    };

    struct ImportStatement : public Statement
    {
        ImportStatement(const Token& name, const std::queue<ExprPtr>& imports, ExprPtr importName, bool isNamed = false) : Statement(), name(name), imports(imports), importName(importName), isNamed(isNamed) {}
        ~ImportStatement() {}

        virtual void accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override {auto p = imports; std::string res = ""; while(p.size() > 0) { res += "[" + p.front()->prettyPrint() + "], "; p.pop();} return "Importing: " + res + (importName != nullptr ? "as" + importName->prettyPrint() : ""); }
        virtual std::string getType() const override { return "ImportStatement"; }

        Token name;
        std::queue<ExprPtr> imports;
        ExprPtr importName;

        bool isNamed;
    };

}