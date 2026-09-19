#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <stdexcept>

#include "expression.h"
#include "statement.h"
#include "environment.h"

namespace pg
{
    //TODO: handle different types of contexts
    class Visitor
    {
    public:
        Visitor(std::shared_ptr<Environment> environment = nullptr) : env(std::make_shared<Environment>(environment)) {}
        virtual ~Visitor() {}

        virtual std::shared_ptr<Valuable> visit(BinaryExpression *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(LogicExpression *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(UnaryExpression *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(PreFixExpression *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(PostFixExpression *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(CompoundAtom *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(Atom *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(List *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(This *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(Var *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(Assign *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(CallExpression *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(Get *expr) = 0;
        virtual std::shared_ptr<Valuable> visit(Set *expr) = 0;

        // Newer AST nodes (bytecode front-end parity). Defaults either execute
        // the node's pre-built desugared form (built by the parser, children
        // shared with the structured form) or throw for visitors that predate
        // the syntax. Only visitors that care (e.g. the bytecode emitter)
        // override these.
        virtual std::shared_ptr<Valuable> visit(AnonymousFunction *expr) { throw std::runtime_error("Anonymous functions are not supported by this visitor (line " + std::to_string(expr->token.line) + ")"); }
        virtual std::shared_ptr<Valuable> visit(IndexGet *expr) { if (expr->desugared) return expr->desugared->accept(this); throw std::runtime_error("IndexGet without desugared form"); }
        virtual std::shared_ptr<Valuable> visit(IndexSet *expr) { if (expr->desugared) return expr->desugared->accept(this); throw std::runtime_error("IndexSet without desugared form"); }

        virtual void visitStatement(ExpressionStatement *stmt) = 0;
        virtual void visitStatement(VariableStatement *stmt) = 0;
        virtual void visitStatement(FunctionStatement *stmt) = 0;
        virtual void visitStatement(ClassStatement *stmt) = 0;
        virtual void visitStatement(BlockStatement *stmt) = 0;
        virtual void visitStatement(IfStatement *stmt) = 0;
        virtual void visitStatement(WhileStatement *stmt) = 0;
        virtual void visitStatement(ReturnStatement *stmt) = 0;
        virtual void visitStatement(ImportStatement *stmt) = 0;

        virtual void visitStatement(ForStatement *stmt) { if (stmt->desugared) { stmt->desugared->accept(this); return; } throw std::runtime_error("ForStatement without desugared form"); }
        virtual void visitStatement(ForInStatement *stmt) { if (stmt->desugared) { stmt->desugared->accept(this); return; } throw std::runtime_error("ForInStatement without desugared form"); }
        virtual void visitStatement(BreakStatement *stmt) { throw std::runtime_error("'break' is not supported by this visitor (line " + std::to_string(stmt->token.line) + ")"); }
        virtual void visitStatement(ContinueStatement *stmt) { throw std::runtime_error("'continue' is not supported by this visitor (line " + std::to_string(stmt->token.line) + ")"); }
        virtual void visitStatement(DPrintStatement *stmt) { throw std::runtime_error("'__dprint' is not supported by this visitor (line " + std::to_string(stmt->token.line) + ")"); }

    protected:
        std::shared_ptr<Environment> env;
    };

    class PgInterpreter;
    class Interpreter;
    class SysModule;
    class VisitorReference;

    class VisitorInterpreter : public Visitor
    {
    friend class Interpreter;
    friend class SysModule;
    friend class VisitorReference;
    public:
        // Keep the base-class defaults visible for the node overloads this
        // visitor does not override (For/ForIn/Index/etc. run their desugared
        // form through the Visitor defaults)
        using Visitor::visit;
        using Visitor::visitStatement;

        VisitorInterpreter(PgInterpreter *interpreter, std::shared_ptr<Environment> environment, const std::unordered_map<Expression*, unsigned int>& localsList, const std::string& scriptName) : Visitor(environment), localsList(localsList), interpreter(interpreter), scriptName(scriptName) {}

        virtual ~VisitorInterpreter() {}

        // Visit for expressions
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

        // Visit for statement
        virtual void visitStatement(ExpressionStatement *stmt) override;
        virtual void visitStatement(VariableStatement *stmt) override;
        virtual void visitStatement(FunctionStatement *stmt) override;
        virtual void visitStatement(ClassStatement *stmt) override;
        virtual void visitStatement(BlockStatement *stmt) override;
        virtual void visitStatement(IfStatement *stmt) override;
        virtual void visitStatement(WhileStatement *stmt) override;
        virtual void visitStatement(ReturnStatement *stmt) override;
        virtual void visitStatement(ImportStatement *stmt) override;

        inline void resetReturnFlags() { returnTriggered = false; }

        inline void setEcsSysFlag() { hasEcsSysFlag = true; }
        
        bool hasEcsSys() const;

        virtual std::shared_ptr<VisitorReference> getVisitorRef();

    protected:
        std::shared_ptr<Environment> globalContext = env;
        std::unordered_map<Expression*, unsigned int> localsList;

        friend std::shared_ptr<Valuable> executeBlock(std::queue<StatementPtr> statements, VisitorInterpreter* visitor, std::shared_ptr<Environment> environment);

        std::shared_ptr<Environment> ancestor(int distance) const;
        std::shared_ptr<Valuable> lookUpVariable(const std::string& name, const Token& token, Expression* expression) const;
        std::shared_ptr<Valuable> getAt(int distance, const std::string& name, const Token& token) const;

        void assignVariable(const Token& name, Expression* expression, std::shared_ptr<Valuable> value);
        void assignAt(int distance, const Token& name, std::shared_ptr<Valuable> value);

        /** Flag to indicate that a return statement was encountered */
        bool returnTriggered = false;

        /** Return value in case a return statement was encountered */
        std::shared_ptr<Valuable> returnValue;

        PgInterpreter *interpreter;

        std::string scriptName;

        // Flag to indicate that a ecs system or event was defined in this script and so it needs to be saved to not invalidate the ecs
        mutable bool hasEcsSysFlag = false;

        // Keep a reference to all imported interpreters to keep their statement ptr valid
        std::vector<std::shared_ptr<Interpreter>> importedInterpreters;
    };

    /**
     * This class is used to create a new visitor that reference a VisitorInterpreter at a certain point
     * 
     * It is mainly used to fake the visitor that it references to allow multiple function concurrently coming from the same script in the ECS
     */
    class VisitorReference : public VisitorInterpreter
    {
    public:
        VisitorReference(VisitorInterpreter *referee) : VisitorInterpreter(referee->interpreter, referee->env, referee->localsList, referee->scriptName), referee(referee) {}

        VisitorReference(const VisitorReference& ref) : VisitorInterpreter(ref.referee->interpreter, ref.referee->env, ref.referee->localsList, ref.referee->scriptName), referee(ref.referee) {}

        virtual ~VisitorReference() {}

        virtual std::shared_ptr<VisitorReference> getVisitorRef() { return std::make_shared<VisitorReference>(*this); }

        std::string getScriptName() const { return scriptName; }

    private:
        VisitorInterpreter *referee;
    };

    std::shared_ptr<Valuable> executeBlock(std::queue<StatementPtr> statements, VisitorInterpreter* visitor, std::shared_ptr<Environment> environment);

    struct ScriptImport
    {
        std::queue<StatementPtr> ast;
        std::unordered_map<Expression*, unsigned int> symbols;
        std::shared_ptr<Environment> env = nullptr;
        std::string name = "";
    };

    class Interpreter
    {
    public:
        Interpreter(const ScriptImport& script, PgInterpreter *interpreter) : localsList(script.symbols), visitor(interpreter, nullptr, localsList, script.name), statements(script.ast) {};

        template<typename Functional>
        void defineSystemFunction(const std::string& name);

        template<typename Functional, typename... Args>
        void defineSystemFunction(const std::string& name, const Args&... args);

        std::shared_ptr<Environment> interpret();

        inline bool hasError() const { return encounteredError; }

        inline bool hasEcsSys() const { return visitor.hasEcsSys(); }

    private:
        std::unordered_map<Expression*, unsigned int> localsList;
        VisitorInterpreter visitor;
        
        std::queue<StatementPtr> statements;
        bool encounteredError = false;
    };

    template<typename Functional>
    void Interpreter::defineSystemFunction(const std::string& name)
    {
        std::queue<ExprPtr> emptyQueue;
        Token token;
        token.text = name;

        auto function = std::make_shared<Functional>(visitor.globalContext, name, token, &visitor, emptyQueue, nullptr);
        // Todo check staticly if type Functional as a setUp function using SFINAE
        function->setUp();

        visitor.globalContext->declareValue(name, function);
    }

    template<typename Functional, typename... Args>
    void Interpreter::defineSystemFunction(const std::string& name, const Args&... args)
    {
        std::queue<ExprPtr> emptyQueue;
        Token token;
        token.text = name;

        auto function = std::make_shared<Functional>(visitor.globalContext, name, token, &visitor, emptyQueue, nullptr);
        // Todo check staticly if type Functional as a setUp function using SFINAE
        function->setUp(args...);

        visitor.globalContext->declareValue(name, function);
    }

    struct SysListElement
    {
        SysListElement(const std::string& str, ValuablePtr type) : key(str), value(type) {}
        SysListElement(const std::string& str, std::shared_ptr<Function> type) : key(str), value(type) {}
        SysListElement(const std::string& str, std::shared_ptr<ClassInstance> type) : key(str), value(type) {}
        SysListElement(const std::string& str, const ElementType& type) : key(str), value(makeVar(type)) {}

        template<typename T>
        SysListElement(const std::string& str, T&& t) : key(str), value(makeVar(std::forward<T>(t))) {}

        std::string key;
        ValuablePtr value;
    };

    std::shared_ptr<ClassInstance> addToList(std::shared_ptr<ClassInstance> instance, const Token& token, const SysListElement& arg);

    template<typename Functional, typename... Args>
    std::shared_ptr<Function> makeFun(const Function *caller, const std::string& name, Args&&... args)
    {
        std::queue<ExprPtr> emptyQueue;
        Token token;
        token.text = name;

        auto function = std::make_shared<Functional>(caller->getEnv(), name, token, caller->getVisitor(), emptyQueue, nullptr);
        function->setUp(std::forward<Args>(args)...);

        return function;
    }

    template<typename Functional>
    std::shared_ptr<Function> makeFun(const Function *caller, const std::string& name)
    {
        std::queue<ExprPtr> emptyQueue;
        Token token;
        token.text = name;

        auto function = std::make_shared<Functional>(caller->getEnv(), name, token, caller->getVisitor(), emptyQueue, nullptr);
        function->setUp();

        return function;
    }

    std::shared_ptr<ClassInstance> addToList(const Function *caller, std::shared_ptr<ClassInstance> instance, const std::vector<SysListElement>& args);

    std::shared_ptr<ClassInstance> makeList(const Function *caller, const std::initializer_list<SysListElement>& list);
}
