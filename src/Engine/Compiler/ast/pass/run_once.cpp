#include "stdafx.h"

#include "run_once.h"

#include "program_view.h"

#include "Compiler/vm.h"
#include "logger.h"

#include <algorithm>
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

    namespace ast
    {
        std::vector<Join> walkConsts(const std::vector<StatementPtr>& statements,
                                     const std::function<bool(const VariableStatement&)>& isConst,
                                     const std::function<int(size_t statement, const std::vector<size_t>& open)>& makes,
                                     bool inOrder)
        {
            std::vector<Join> out(statements.size());

            // The consts the statements since the last other one declared, in their order: a fill of
            // one joins it
            std::vector<size_t> open;
            int lastJoined = -1;

            auto close = [&]()
            {
                open.clear();
                lastJoined = -1;
            };

            auto openNamed = [&](const std::string& name) -> int
            {
                // A later declaration of the name wins
                for (auto it = open.rbegin(); it != open.rend(); ++it)
                {
                    if (std::static_pointer_cast<VariableStatement>(statements[*it])->name.text == name)
                        return static_cast<int>(*it);
                }

                return -1;
            };

            for (size_t i = 0; i < statements.size(); ++i)
            {
                const StatementPtr& s = statements[i];

                // A definition runs no code: the fills after it still follow their const
                if (s and s->kind() == AstKind::FunctionStatement)
                    continue;

                if (s and s->kind() == AstKind::VariableStatement)
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);

                    if (isConst(*v) and v->expr)
                    {
                        // An initializer that may read the consts before it: their later fills would
                        // be seen by it too early, joined to them. What joined it runs with it
                        const bool constantFills = std::all_of(v->keptFills.begin(), v->keptFills.end(), [](const StatementPtr& fill) { return not filledConst(fill).empty(); });

                        if (not isConstantData(v->expr) or not constantFills)
                            close();

                        open.push_back(i);
                        continue;
                    }
                }

                const std::string filled = filledConst(s);
                int target = filled.empty() ? -1 : openNamed(filled);

                Join& join = out[i];
                join.fill = target >= 0;

                // What makes the open consts (a loop putting their tables in order)
                if (target < 0 and not open.empty())
                    target = makes(i, open);

                if (target >= 0 and (not inOrder or target >= lastJoined))
                {
                    join.joined = true;
                    join.target = static_cast<size_t>(target);
                    lastJoined = target;

                    for (auto o : open)
                        join.open.insert(std::static_pointer_cast<VariableStatement>(statements[o])->name.text);

                    continue;
                }

                // Anything else may read a const: what comes after it fills no initializer
                join.fill = false;
                close();
            }

            return out;
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
        }

        bool changed = false;
        size_t kept = 0;
        size_t joined = 0;

        const std::vector<StatementPtr> all = listOf(statements);

        const std::vector<ast::Join> joins = ast::walkConsts(all,
            [](const VariableStatement& v) { return v.isConst and not v.inferredConst; },
            [](size_t, const std::vector<size_t>&) { return -1; }, false);

        std::vector<StatementPtr> out;

        for (size_t i = 0; i < all.size(); ++i)
        {
            const StatementPtr& s = all[i];

            if (s and s->kind() == AstKind::FunctionStatement)
            {
                auto f = std::static_pointer_cast<FunctionStatement>(s);

                if (keptFunctions.count(f->name.text) and not f->keptOnce)
                {
                    f->keptOnce = true;
                    changed = true;
                    ++kept;
                }
            }

            if (joins[i].joined)
            {
                std::static_pointer_cast<VariableStatement>(all[joins[i].target])->keptFills.push_back(s);
                changed = true;
                ++joined;
                continue;
            }

            out.push_back(s);
        }

        statements = queueOf(out);

        LOG_INFO(DOM, "Kept " << kept << " functions, joined " << joined << " fills to their const");

        return changed;
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
