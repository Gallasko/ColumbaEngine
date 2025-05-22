#include "stdafx.h"

#include "resolver.h"

#include "parser.h"

#include "logger.h"

namespace pg
{

    namespace
    {
        const char * DOM = "Resolver";
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(BinaryExpression *expr)
    {
        expr->leftExpr->accept(this);
        expr->rightExpr->accept(this);

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(LogicExpression *expr)
    {
        expr->leftExpr->accept(this);
        expr->rightExpr->accept(this);

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(UnaryExpression *expr)
    {
        expr->expr->accept(this);

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(PreFixExpression *expr)
    {
        expr->expr->accept(this);
        resolveLocal(expr, expr->getName());

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(PostFixExpression *expr)
    {
        expr->expr->accept(this);
        resolveLocal(expr, expr->getName());

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(CompoundAtom *expr)
    {
        expr->expr->accept(this);

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(Atom*)
    {
        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(List* expr)
    {
        scopes.push(std::unordered_map<std::string, bool>());
        scopes.top()["this"] = true;
        resolveLocal(expr->self.get(), expr->self->getName());

        auto entries = expr->entries;

        while(entries.size() > 0)
        {
            auto entry = entries.front();

            entry.key->accept(this);
            entry.value->accept(this);

            entries.pop();
        }

        scopes.pop();

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(This *expr)
    {
        if(currentClass == ClassType::NONE)
            throw ParseException(expr->name, "Can't use 'this' outside a class");

        resolveLocal(expr, expr->getName());
        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(Var *expr)
    {
        if(not scopes.empty())
        {
            const auto it = scopes.top().find(expr->name.text);

            if(it != scopes.top().end() && it->second == false)
                throw ParseException(expr->name, "Can't read local variable inside is own initializer");
        }

        resolveLocal(expr, expr->getName());

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(Assign *expr)
    {
        expr->expr->accept(this);
        resolveLocal(expr, expr->getName());

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(CallExpression *expr)
    {
        expr->caller->accept(this);

        std::queue<ExprPtr> temp = expr->args;

        while(temp.size() > 0)
        {
            temp.front()->accept(this);

            temp.pop();
        }

        resolveLocal(expr, expr->caller->getName());

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(Get *expr)
    {
        expr->object->accept(this);

        return nullptr;
    }

    std::shared_ptr<Valuable> VisitorResolver::visit(Set *expr)
    {
        expr->object->accept(this);
        expr->value->accept(this);

        return nullptr;
    }

    void VisitorResolver::visitStatement(ExpressionStatement *stmt)
    {
        stmt->expr->accept(this);
    }

    void VisitorResolver::visitStatement(VariableStatement *stmt)
    {
        declare(stmt->name.text);
        if(stmt->expr)
            stmt->expr->accept(this);

        define(stmt->name.text);
    }

    void VisitorResolver::visitStatement(FunctionStatement *stmt)
    {
        declare(stmt->name.text);
        define(stmt->name.text);

        resolveFunction(stmt, FunctionType::FUNCTION);
    }

    void VisitorResolver::visitStatement(ClassStatement *stmt)
    {
        ClassType enclosingClass = currentClass;
        currentClass = ClassType::CLASS;

        declare(stmt->name.text);
        define(stmt->name.text);

        scopes.push(std::unordered_map<std::string, bool>());
        scopes.top()["this"] = true;

        auto tmpMethods = stmt->methods;

        while(tmpMethods.size() > 0)
        {
            resolveFunction(tmpMethods.front().get(), FunctionType::METHOD);
            tmpMethods.pop();
        }

        scopes.pop();

        currentClass = enclosingClass;
    }

    void VisitorResolver::visitStatement(BlockStatement *stmt)
    {
        scopes.push(std::unordered_map<std::string, bool>());

        auto temp = stmt->statements;

        while(temp.size() > 0)
        {
            temp.front()->accept(this);
            temp.pop();
        }

        scopes.pop();
    }

    void VisitorResolver::visitStatement(IfStatement *stmt)
    {
        stmt->condition->accept(this);
        stmt->thenBranch->accept(this);
        if(stmt->elseBranch) stmt->elseBranch->accept(this);
    }

    void VisitorResolver::visitStatement(WhileStatement *stmt)
    {
        stmt->condition->accept(this);
        stmt->body->accept(this);
    }

    void VisitorResolver::visitStatement(ReturnStatement *stmt)
    {
        if(currentFunction == FunctionType::NONE)
            throw ParseException(stmt->name, "Can't return from top-level code.");

        if(stmt->value)
            stmt->value->accept(this);
    }

    void VisitorResolver::visitStatement(ImportStatement *stmt)
    {
        auto tmpImports = stmt->imports;

        if(stmt->isNamed)
        {
            LOG_ERROR(DOM, "Named imports are not supported yet.");
            // auto import = tmpImports.front();
            // auto importName = import->getName();

            // declare(importName);
            // define(importName);
        }
        else
        {
            while(tmpImports.size() > 0)
            {
                auto import = tmpImports.front();

                import->accept(this);

                tmpImports.pop();
            }
        }
    }

    void VisitorResolver::declare(const std::string& name)
    {
        if(scopes.empty()) return;

        scopes.top().emplace(name, false);
    }

    void VisitorResolver::define(const std::string& name)
    {
        if(scopes.empty()) return;

        scopes.top()[name] = true;
    }

    void VisitorResolver::resolveLocal(Expression* expression, const std::string& name)
    {
        for(int i = scopes.size() - 1; i >= 0; i--)
        {
            const auto& scope = scopes.at(i);
            if(scope.find(name) != scope.end())
            {
                locals.emplace(expression, scopes.size() - 1 - i);
                return;
            }
        }
    }

    void VisitorResolver::resolveFunction(FunctionStatement* statement, const FunctionType& type)
    {
        FunctionType enclosingFunction = currentFunction;
        currentFunction = type;

        scopes.push(std::unordered_map<std::string, bool>());

        auto temp = statement->parameters;

        while(temp.size() > 0)
        {
            auto paramName = temp.front()->getName();
            declare(paramName);
            define(paramName);

            temp.pop();
        }

        statement->body->accept(this);

        scopes.pop();

        currentFunction = enclosingFunction;
    }

    const std::unordered_map<Expression*, unsigned int>& Resolver::resolve()
    {
        std::queue<StatementPtr> temp;

        while(not statements.empty())
        {
            auto stmt = statements.front();
            temp.push(stmt);
            statements.pop();

            try
            {
                if(temp.back())
                    temp.back()->accept(&rVisitor);
            }
            catch(const std::exception& e)
            {
                LOG_ERROR(DOM, e.what());
                errorEncountered = true;
            }
        }

        statements = temp;

        return rVisitor.getLocals();
    }

}
