#include "stdafx.h"

#include "run_once.h"

#include "program_view.h"

#include "Compiler/vm.h"
#include "logger.h"

#include <map>
#include <vector>

namespace pg
{

    namespace
    {
        const char* DOM = "RunOnce";

        template <typename T>
        std::vector<T> listOf(std::queue<T> q)
        {
            std::vector<T> out;

            while (not q.empty())
            {
                out.push_back(q.front());
                q.pop();
            }

            return out;
        }

        template <typename T>
        std::queue<T> queueOf(const std::vector<T>& v)
        {
            std::queue<T> q;

            for (const auto& x : v)
                q.push(x);

            return q;
        }

        bool isConstantValue(const ExprPtr& e)
        {
            return e and (e->kind() == AstKind::Atom or e->kind() == AstKind::NoneAtom);
        }

        // A value made of constants only (constants, negated ones, lists and tables of them): it
        // reads nothing, so no earlier const
        bool isConstantData(const ExprPtr& e)
        {
            if (isConstantValue(e))
                return true;

            if (e and e->kind() == AstKind::UnaryExpression)
                return isConstantValue(std::static_pointer_cast<UnaryExpression>(e)->expr);

            if (e and e->kind() == AstKind::List)
            {
                for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                {
                    // A key written `key: value` is its name, emitted as a string constant
                    const bool constantKey = not entry.key or entry.explicitKey or isConstantData(entry.key);

                    if (not constantKey or not isConstantData(entry.value))
                        return false;
                }

                return true;
            }

            return false;
        }

        std::string varName(const ExprPtr& e)
        {
            if (e and e->kind() == AstKind::Var)
                return std::static_pointer_cast<Var>(e)->name.text;

            return "";
        }

        // The const a statement fills with a constant entry (`t["key"] = value`, `t.key = value`),
        // "" when it is no such fill
        std::string filledConst(const StatementPtr& s)
        {
            if (not s or s->kind() != AstKind::ExpressionStatement)
                return "";

            const ExprPtr& e = std::static_pointer_cast<ExpressionStatement>(s)->expr;

            if (not e)
                return "";

            if (e->kind() == AstKind::IndexSet)
            {
                auto set = std::static_pointer_cast<IndexSet>(e);

                if (isConstantValue(set->index) and isConstantValue(set->value))
                    return varName(set->object);
            }
            else if (e->kind() == AstKind::Set)
            {
                auto set = std::static_pointer_cast<Set>(e);

                if (isConstantValue(set->value))
                    return varName(set->object);
            }

            return "";
        }
    }

    bool RunOncePass::runPass(VM* vm, std::queue<StatementPtr>& statements)
    {
        // The whole program: the script compiled (outermost) and every module it imports. A module
        // compiled while it is (deeper) belongs to that program
        if (not vm or vm->astCompileDepth <= 1)
        {
            keptFunctions.clear();

            const ast::ProgramView program = ast::readProgram(vm, statements);

            if (program.complete)
            {
                for (const auto& [name, count] : program.defined)
                {
                    if (count == 1 and not program.poisoned.count(name))
                        keptFunctions.insert(name);
                }
            }

            analyzed = false;
            effects = ast::ProgramEffects();

            if (wholeProgram)
            {
                const ast::ProgramFiles files = ast::readProgramFiles(vm, statements);

                analyzed = files.complete;

                if (analyzed)
                    analyzed = effects.analyze(files.files);
            }
        }

        bool changed = false;
        size_t kept = 0;
        size_t joined = 0;
        size_t inferred = 0;
        size_t made = 0;

        std::vector<StatementPtr> out;

        // The consts the statements since the last other one declared, in their order: a fill of
        // one joins it
        std::vector<std::shared_ptr<VariableStatement>> open;

        auto openNamed = [&open](const std::string& name) -> std::shared_ptr<VariableStatement>
        {
            for (const auto& v : open)
            {
                if (v->name.text == name)
                    return v;
            }

            return nullptr;
        };

        for (const auto& s : listOf(statements))
        {
            if (s and s->kind() == AstKind::FunctionStatement)
            {
                auto f = std::static_pointer_cast<FunctionStatement>(s);

                if (keptFunctions.count(f->name.text) and not f->keptOnce)
                {
                    f->keptOnce = true;
                    changed = true;
                    ++kept;
                }

                // A definition runs no code: the fills after it still follow their const
                out.push_back(s);
                continue;
            }

            if (s and s->kind() == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                // A var found to be as good as a const is one
                if (not v->isConst and v->expr and analyzed and effects.inferredConsts().count(v->name.text))
                {
                    v->isConst = true;
                    changed = true;
                    ++inferred;
                }

                if (v->isConst and v->expr)
                {
                    // An initializer that may read the consts before it: their later fills would
                    // be seen by it too early, joined to them
                    if (not isConstantData(v->expr))
                        open.clear();

                    open.push_back(v);
                    out.push_back(s);
                    continue;
                }
            }

            const std::string target = filledConst(s);

            if (auto filled = target.empty() ? nullptr : openNamed(target))
            {
                filled->keptFills.push_back(s);
                changed = true;
                ++joined;
                continue;
            }

            // What makes the open consts (a loop putting their tables in order): with the last of them it uses
            if (auto making = open.empty() ? nullptr : joinTarget(s, open))
            {
                making->keptFills.push_back(s);
                changed = true;
                ++made;
                continue;
            }

            // Anything else may read a const: what comes after it fills no initializer
            open.clear();
            out.push_back(s);
        }

        statements = queueOf(out);

        LOG_INFO(DOM, "Kept " << kept << " functions, " << inferred << " vars made consts, joined " << joined << " fills and "
                 << made << " statements to their const");

        return changed;
    }

    std::shared_ptr<VariableStatement> RunOncePass::joinTarget(const StatementPtr& s, const std::vector<std::shared_ptr<VariableStatement>>& open) const
    {
        if (not analyzed or not s)
            return nullptr;

        switch (s->kind())
        {
            case AstKind::ExpressionStatement:
            case AstKind::ForStatement:
            case AstKind::ForInStatement:
            case AstKind::WhileStatement:
            case AstKind::IfStatement:
            case AstKind::BlockStatement:
                break;

            default:
                return nullptr; // A definition is no part of making another
        }

        const ast::StatementEffects e = effects.effectsOf(s);

        std::set<std::string> openNames;

        for (const auto& v : open)
            openNames.insert(v->name.text);

        if (not effects.onlyMakes(e, openNames))
            return nullptr;

        // It reads the consts whose data it writes (itself or through what it calls): the last
        // of those it reads is where it joins, after all it uses is made
        for (auto it = open.rbegin(); it != open.rend(); ++it)
        {
            if (e.reads.count((*it)->name.text))
                return *it;
        }

        return nullptr;
    }

    std::string RunOncePass::factsAboutIdentifiers(const std::set<std::string>& identifiers) const
    {
        // What its choices for a module (consts inferred, statements joined) rest on: of every
        // name the module holds, what the program says of it
        if (not wholeProgram)
            return "";

        if (not analyzed)
            return "unread";

        std::string facts;

        for (const auto& name : identifiers)
        {
            const std::string flags = effects.factsOf(name);

            if (not flags.empty())
                facts += name + ":" + flags + ";";
        }

        return facts;
    }

    std::string RunOncePass::factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>&) const
    {
        // Which of the module's functions (their names are among those it calls: `fun name(`) the
        // program keeps from run to run
        std::string facts;

        for (const auto& name : names)
        {
            if (keptFunctions.count(name))
                facts += name + ";";
        }

        return facts;
    }
}
