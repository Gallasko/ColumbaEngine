#include "stdafx.h"

#include "program_consts.h"

#include "program_view.h"

#include "Compiler/vm.h"
#include "logger.h"

#include <vector>

namespace pg
{
    namespace
    {
        const char* DOM = "ProgramConsts";
    }

    bool ProgramConstsPass::runPass(VM* vm, std::queue<StatementPtr>& statements)
    {
        // The whole program: the script compiled (outermost) and every module it imports. A module
        // compiled while it is (deeper) belongs to that program
        const bool script = not vm or vm->astCompileDepth <= 1;

        if (script)
        {
            effects = ast::ProgramEffects();

            const ast::ProgramFiles files = ast::readProgramFiles(vm, statements);

            analyzed = files.complete;

            if (analyzed)
                analyzed = effects.analyze(files.files, files.imports);
        }

        if (not analyzed)
            return false;

        // By index, the empty statements of the parser left aside
        std::vector<StatementPtr> all, own;

        for (; not statements.empty(); statements.pop())
        {
            all.push_back(statements.front());

            if (all.back())
                own.push_back(all.back());
        }

        const int file = script ? 0 : effects.fileOf(own);

        if (file < 0)
        {
            if (file == ast::ProgramEffects::TwinFiles)
                LOG_INFO(DOM, "A module two files of the program could be: nothing was decided for it");
            else
                LOG_ERROR(DOM, "A module does not line up with the file analyzed: nothing is applied to it");

            for (const auto& s : all)
                statements.push(s);

            return false;
        }

        const std::vector<ast::Join>& joins = effects.joinsInFile(file);

        bool changed = false;
        size_t inferred = 0;
        size_t joined = 0;

        size_t index = 0;

        for (const auto& s : all)
        {
            if (not s)
            {
                statements.push(s);
                continue;
            }

            const ast::Join& join = joins[index++];

            // A var found to be as good as a const is one
            if (s->kind() == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                if (not v->isConst and v->expr and effects.inferredConsts().count(v->name.text))
                {
                    v->isConst = true;
                    v->inferredConst = true;
                    changed = true;
                    ++inferred;
                }
            }

            if (join.joined)
            {
                std::static_pointer_cast<VariableStatement>(own[join.target])->keptFills.push_back(s);
                changed = true;
                ++joined;
                continue;
            }

            statements.push(s);
        }

        LOG_INFO(DOM, inferred << " vars made consts, joined " << joined << " statements to their const");

        return changed;
    }

    std::string ProgramConstsPass::factsAboutIdentifiers(const std::set<std::string>& identifiers) const
    {
        // What its choices for a module rest on: of every name the module holds, what the program
        // says of it
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
}
