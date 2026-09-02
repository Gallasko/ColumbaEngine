#pragma once

#include <memory>
#include <queue>

#include "token.h"
#include "Memory/elementtype.h"

namespace pg
{
    // Forward declarations
    class Visitor;
    class Valuable;
    class Statement;

    /**
     * @class Expression
     * 
     * 
     */
    class Expression
    {
    public:
        Expression() {}
        virtual ~Expression() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) = 0;
        virtual std::string prettyPrint() const = 0;
        virtual std::string getName() const = 0;
        virtual std::string getType() const = 0;
    };

    typedef std::shared_ptr<Expression> ExprPtr;

    struct ListElement
    {
        ExprPtr key;
        ExprPtr value;

        /** True when the entry was written as 'key: value' (explicit keys emit as string constants on the VM side) */
        bool explicitKey = false;
    };

    struct BinaryExpression : public Expression
    {
        BinaryExpression(ExprPtr leftExpr, const Token& token, ExprPtr rightExpr) : Expression(), leftExpr(leftExpr), op(token), rightExpr(rightExpr) {}
        ~BinaryExpression() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return leftExpr->prettyPrint() + " " + op.text + " " + rightExpr->prettyPrint(); }
        virtual std::string getName() const override { return op.text; }
        virtual std::string getType() const override { return "BinaryExpression"; }

        ExprPtr leftExpr;
        Token op;
        ExprPtr rightExpr;    
    };

    struct LogicExpression : public Expression
    {
        LogicExpression(ExprPtr leftExpr, const Token& token, ExprPtr rightExpr) : Expression(), leftExpr(leftExpr), op(token), rightExpr(rightExpr) {}
        ~LogicExpression() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return leftExpr->prettyPrint() + " " + op.text + " " + rightExpr->prettyPrint(); }
        virtual std::string getName() const override { return op.text; }
        virtual std::string getType() const override { return "LogicExpression"; }

        ExprPtr leftExpr;
        Token op;
        ExprPtr rightExpr;    
    };

    struct UnaryExpression : public Expression
    {
        UnaryExpression(ExprPtr expr, const Token& token) : Expression(), op(token), expr(expr) {}
        ~UnaryExpression() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return op.text + " " + expr->prettyPrint(); }
        virtual std::string getName() const override { return op.text; }
        virtual std::string getType() const override { return "UnaryExpression"; }

        Token op;
        ExprPtr expr;
    };

    struct PreFixExpression : public Expression
    {
        PreFixExpression(ExprPtr expr, const Token& token, const Token& name) : Expression(), op(token), name(name), expr(expr) {}
        ~PreFixExpression() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return name.text + " " + expr->prettyPrint(); }
        virtual std::string getName() const override { return name.text; }
        virtual std::string getType() const override { return "PreFixExpression"; }
        
        Token op;
        Token name;
        ExprPtr expr;
    };

    struct PostFixExpression : public Expression
    {
        PostFixExpression(ExprPtr expr, const Token& token, const Token& name) : Expression(), op(token), name(name), expr(expr) {}
        ~PostFixExpression() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return name.text + " " + expr->prettyPrint(); }
        virtual std::string getName() const override { return name.text; }
        virtual std::string getType() const override { return "PostFixExpression"; }

        Token op;
        Token name;
        ExprPtr expr;
    };

    struct CompoundAtom : public Expression
    {
        CompoundAtom(ExprPtr expr) : Expression(), expr(expr) {}
        ~CompoundAtom() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return expr->prettyPrint(); }
        virtual std::string getName() const override { return expr->getName(); }
        virtual std::string getType() const override { return "CompoundAtom"; }

        ExprPtr expr;
    };

    struct Atom : public Expression
    {
        template <typename Type>
        explicit Atom(const Type& value) : Expression(), value(value) { }
        ~Atom() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return value.toString(); }
        virtual std::string getName() const override { return value.toString(); }
        virtual std::string getType() const override { return "Atom"; }

        ElementType value;
    };

    struct List : public Expression
    {
        List(ExprPtr self, const Token& token, const std::queue<ListElement>& elements) : Expression(), self(self), squareBracket(token), entries(elements) { }
        ~List() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { auto a = entries; std::string res = ""; while(a.size() > 0) { res += "[" + a.front().key->prettyPrint() + "]: " + a.front().value->prettyPrint() + ", "; a.pop();}  return "List Node with values: " + res; }
        virtual std::string getName() const override { return "List"; }
        virtual std::string getType() const override { return "List"; }

        ExprPtr self;
        Token squareBracket;
        std::queue<ListElement> entries;

        /** True when the literal was written with braces '{...}' (always a table on the VM side) */
        bool braceForm = false;

        /** True when at least one entry used an explicit 'key: value' form */
        bool hasExplicitKeys = false;
    };

    /**
     * Anonymous function expression: fun(params) { body }
     *
     * Only supported by the bytecode front-end; the tree-walking interpreter
     * predates this syntax and never executes it.
     */
    struct AnonymousFunction : public Expression
    {
        AnonymousFunction(const Token& token, const std::queue<ExprPtr>& parameters, std::shared_ptr<Statement> body) : Expression(), token(token), parameters(parameters), body(body) {}
        ~AnonymousFunction() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { auto p = parameters; std::string res = ""; while (p.size() > 0) { res += p.front()->prettyPrint() + ", "; p.pop(); } return "Anonymous function with parameters: " + res; }
        virtual std::string getName() const override { return "fun"; }
        virtual std::string getType() const override { return "AnonymousFunction"; }

        Token token;
        std::queue<ExprPtr> parameters;
        std::shared_ptr<Statement> body;
    };

    /**
     * Subscript read: object[index]
     *
     * The structured fields are used by the bytecode front-end; 'desugared'
     * holds the equivalent 'object.at(index)' call chain executed by the
     * tree-walking interpreter and resolved by the resolver (children are
     * shared between both forms).
     */
    struct IndexGet : public Expression
    {
        IndexGet(ExprPtr object, ExprPtr index, const Token& bracket) : Expression(), object(object), index(index), bracket(bracket) {}
        ~IndexGet() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Index get: " + object->prettyPrint() + "[" + index->prettyPrint() + "]"; }
        virtual std::string getName() const override { return "IndexGet"; }
        virtual std::string getType() const override { return "IndexGet"; }

        ExprPtr object;
        ExprPtr index;
        Token bracket;

        /** Equivalent 'object.at(index)' call for the tree-walking path */
        ExprPtr desugared;
    };

    /**
     * Subscript write: object[index] = value
     *
     * Same structured/desugared split as IndexGet; 'desugared' holds the
     * equivalent 'object.set(index, value)' call chain.
     */
    struct IndexSet : public Expression
    {
        IndexSet(ExprPtr object, ExprPtr index, ExprPtr value, const Token& bracket) : Expression(), object(object), index(index), value(value), bracket(bracket) {}
        ~IndexSet() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Index set: " + object->prettyPrint() + "[" + index->prettyPrint() + "] = " + value->prettyPrint(); }
        virtual std::string getName() const override { return "IndexSet"; }
        virtual std::string getType() const override { return "IndexSet"; }

        ExprPtr object;
        ExprPtr index;
        ExprPtr value;
        Token bracket;

        /** Equivalent 'object.set(index, value)' call for the tree-walking path */
        ExprPtr desugared;
    };

    struct This : public Expression
    {
        explicit This(const Token& token) : Expression(), name(token) { }
        ~This() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "This."; }
        virtual std::string getName() const override { return name.text; }
        virtual std::string getType() const override { return "This"; }

        Token name;
    };

    struct Var : public Expression
    {
        explicit Var(const Token& token) : Expression(), name(token) { }
        ~Var() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Variable '" + name.text + "'."; }
        virtual std::string getName() const override { return name.text; }
        virtual std::string getType() const override { return "Var"; }

        Token name;
    };

    struct Assign : public Expression
    {
        explicit Assign(const Token& token, ExprPtr expr) : Expression(), name(token), expr(expr) { }
        ~Assign() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Assign: " + expr->prettyPrint() + " to variable '" + name.text + "'."; }
        virtual std::string getName() const override { return name.text; }
        virtual std::string getType() const override { return "Assign"; }

        Token name;
        ExprPtr expr;
    };

    struct CallExpression : public Expression
    {
        CallExpression(ExprPtr caller, const Token& paren, const std::queue<ExprPtr>& args) : Expression(), caller(caller), paren(paren), args(args) {}
        ~CallExpression() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { auto a = args; std::string res = ""; while(a.size() > 0) { res += a.front()->prettyPrint() + ", "; a.pop();}  return "Function call: " + caller->prettyPrint() + " with arguments: " + res; }
        virtual std::string getName() const override { return caller->getName(); }
        virtual std::string getType() const override { return "CallExpression"; }

        ExprPtr caller;
        Token paren;
        std::queue<ExprPtr> args;
    };

    struct Get : public Expression
    {
        Get(ExprPtr object, const Token& name) : Expression(), object(object), name(name) {}
        ~Get() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Get property: " + name.text + " of object: " + object->prettyPrint(); }
        virtual std::string getName() const override { return name.text; }
        virtual std::string getType() const override { return "Get"; }

        ExprPtr object;
        Token name;
    };

    struct Set : public Expression
    {
        Set(ExprPtr object, const Token& name, ExprPtr value) : Expression(), object(object), name(name), value(value) {}
        ~Set() {}

        virtual std::shared_ptr<Valuable> accept(Visitor* visitor) override;
        virtual std::string prettyPrint() const override { return "Set property: " + name.text + " of object: " + object->prettyPrint(); }
        virtual std::string getName() const override { return name.text; }
        virtual std::string getType() const override { return "Set"; }

        ExprPtr object;
        Token name;
        ExprPtr value;
    };
}