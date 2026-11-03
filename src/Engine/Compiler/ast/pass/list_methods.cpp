#include "stdafx.h"

#include "list_methods.h"

#include "ast_analysis.h"
#include "loop_ownership.h"
#include "program_view.h"

#include "Compiler/vm.h"
#include "logger.h"

#include <map>

namespace pg
{
    namespace
    {
        using namespace ast;

        const char* DOM = "ListMethods";

        // The arguments each method takes, besides the list
        const std::map<std::string, size_t> arities = {{"push", 1}, {"pop", 0}, {"insert", 2}, {"removeAt", 1}, {"len", 0}};

        // The methods of every class a statement defines, in the functions and methods it holds too
        // (the function values of its expressions are the caller's to give)
        void classMethods(const StatementPtr& s, std::set<std::string>& out)
        {
            if (not s)
                return;

            switch (s->kind())
            {
                case AstKind::BlockStatement:
                    for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                        classMethods(b, out);
                    break;

                case AstKind::IfStatement:
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);
                    classMethods(i->thenBranch, out);
                    classMethods(i->elseBranch, out);
                    break;
                }

                case AstKind::WhileStatement: classMethods(std::static_pointer_cast<WhileStatement>(s)->body, out); break;
                case AstKind::ForStatement: classMethods(std::static_pointer_cast<ForStatement>(s)->body, out); break;
                case AstKind::ForInStatement: classMethods(std::static_pointer_cast<ForInStatement>(s)->body, out); break;
                case AstKind::FunctionStatement: classMethods(std::static_pointer_cast<FunctionStatement>(s)->body, out); break;

                case AstKind::ClassStatement:
                    for (const auto& m : listOf(std::static_pointer_cast<ClassStatement>(s)->methods))
                    {
                        out.insert(m->name.text);
                        classMethods(m->body, out);
                    }
                    break;

                default:
                    break;
            }
        }

        // The list methods a call can run nothing but, in a program read whole: no class has a
        // method of the name and no field can have it
        std::set<std::string> methodsAlone(const ProgramFiles& program)
        {
            if (not program.complete)
                return {};

            std::set<std::string> taken;

            const ExprVisit look = [&](const ExprPtr& e) {
                const AstKind type = e->kind();

                if (type == AstKind::Set)
                    taken.insert(std::static_pointer_cast<Set>(e)->name.text);
                else if (type == AstKind::Atom)
                {
                    const auto& value = std::static_pointer_cast<Atom>(e)->value;

                    if (value.type == UnionType::STRING)
                        taken.insert(value.toString());
                }
                else if (type == AstKind::List)
                {
                    for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                    {
                        if (entry.explicitKey and entry.key)
                            taken.insert(entry.key->getName());
                    }
                }
                else if (type == AstKind::AnonymousFunction)
                    classMethods(std::static_pointer_cast<AnonymousFunction>(e)->body, taken);
            };

            for (const auto& file : program.files)
            {
                for (const auto& s : file)
                {
                    classMethods(s, taken);
                    eachExpr(s, look, true);
                }
            }

            std::set<std::string> out;

            for (const auto& [method, arity] : arities)
            {
                if (not taken.count(method))
                    out.insert(method);
            }

            return out;
        }
    }

    bool ListMethodsPass::runPass(VM* vm, std::queue<StatementPtr>& statements)
    {
        // The whole program: the script compiled (outermost) and every module it imports. A module
        // compiled while it is (deeper) belongs to that program
        if (not vm or vm->astCompileDepth <= 1)
            callable = methodsAlone(readProgramFiles(vm, statements));

        rewritten = 0;

        if (callable.empty())
            return false;

        // Each call before what it holds: `a.push(b.pop())` gives `b.pop()` as an argument, seen next
        const ExprVisit rewrite = [&](const ExprPtr& e) {
            if (e->kind() != AstKind::CallExpression)
                return;

            auto c = std::static_pointer_cast<CallExpression>(e);

            if (not c->caller or c->caller->kind() != AstKind::Get)
                return;

            auto g = std::static_pointer_cast<Get>(c->caller);
            const std::string& method = g->name.text;

            if (not callable.count(method) or c->args.size() != arities.at(method))
                return;

            std::queue<ExprPtr> args;
            args.push(g->object);

            for (const auto& a : listOf(c->args))
                args.push(a);

            c->caller = std::make_shared<Var>(Token(TokenType::EXPRESSION, listMethodGlobal(method), g->name.line, g->name.column));
            c->args = args;

            ++rewritten;
        };

        for (const auto& s : listOf(statements))
            eachExpr(s, rewrite, true);

        LOG_INFO(DOM, "Rewrote " << rewritten << " list method calls");

        return rewritten > 0;
    }

    std::string ListMethodsPass::factsAbout(const std::set<std::string>&, std::vector<std::shared_ptr<const void>>&) const
    {
        std::string facts;

        for (const auto& method : callable)
            facts += method + ";";

        return facts;
    }
}
