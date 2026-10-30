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

        const std::vector<StatementPtr> all = listOf(statements);

        // A module's own parse, read as the one analyzed
        if (analyzed and vm and vm->astCompileDepth > 1)
            effects.readModule(all);

        const std::vector<ast::Join> joins = effects.joinsOf(all);

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
            else if (s and s->kind() == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                // A var found to be as good as a const is one
                if (not v->isConst and v->expr and analyzed and effects.inferredConsts().count(v->name.text))
                {
                    v->isConst = true;
                    changed = true;
                    ++inferred;
                }
            }

            if (joins[i].target)
            {
                joins[i].target->keptFills.push_back(s);
                changed = true;
                ++(joins[i].fill ? joined : made);
                continue;
            }

            out.push_back(s);
        }

        statements = queueOf(out);

        LOG_INFO(DOM, "Kept " << kept << " functions, " << inferred << " vars made consts, joined " << joined << " fills and "
                 << made << " statements to their const");

        return changed;
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
