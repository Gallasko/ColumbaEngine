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

        std::vector<std::string> constexprsOf(const std::vector<std::vector<StatementPtr>>& files)
        {
            std::vector<std::string> names;

            for (const auto& file : files)
            {
                for (const auto& s : file)
                {
                    if (s and s->kind() == AstKind::VariableStatement and std::static_pointer_cast<VariableStatement>(s)->isConstexpr)
                        names.push_back(std::static_pointer_cast<VariableStatement>(s)->name.text);
                }
            }

            return names;
        }
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

            std::vector<std::string> declared = constexprsOf(files.files);

            // Optimizations off, it is here for the constexprs alone
            enabled = (everything and (not vm or vm->enableOptimizations)) or not declared.empty();
            analyzed = enabled and files.complete;

            if (analyzed)
                analyzed = effects.analyze(files.files, files.imports, files.origins);

            if (not analyzed)
            {
                const std::string why = not files.complete ? "the whole program cannot be read (a module with only a .pgc, or an import below the top level of a file)" : effects.refusal();

                for (const auto& name : declared)
                    errors.push_back("constexpr `" + name + "` cannot be proved: " + why);
            }
            else
            {
                for (const auto& problem : effects.constexprProblems())
                    errors.push_back(problem.message);
            }
        }

        if (not enabled)
            return false;

        // By index, the empty statements of the parser left aside
        std::vector<StatementPtr> all, own;

        for (; not statements.empty(); statements.pop())
        {
            all.push_back(statements.front());

            if (all.back())
                own.push_back(all.back());
        }

        // A file nothing was decided for gets the plain walk's fills, what RunOncePass joins without
        // the analysis: it joins none itself here (RunOncePass::fills)
        int file = analyzed ? (script ? 0 : effects.fileOf(own)) : ast::ProgramEffects::NoFile;

        if (analyzed and file == ast::ProgramEffects::TwinFiles)
            LOG_INFO(DOM, "A module two files of the program could be: it gets the plain fills");
        else if (analyzed and file == ast::ProgramEffects::NoFile)
            LOG_ERROR(DOM, "A module does not line up with the file analyzed: it gets the plain fills");
        else if (analyzed and not effects.decided(static_cast<size_t>(file)))
            file = ast::ProgramEffects::TwinFiles;

        const bool plain = file < 0;
        const std::vector<ast::Join> joins = plain ? ast::plainJoins(own) : effects.joinsInFile(static_cast<size_t>(file));

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
            if (not plain and s->kind() == AstKind::VariableStatement)
            {
                auto v = std::static_pointer_cast<VariableStatement>(s);

                if (not v->isConst and v->expr and effects.inferredConsts().count(v->name.text))
                {
                    v->isConst = true;
                    v->inferredConst = true;
                    changed = true;
                    ++inferred;
                }

                v->proven = v->isConstexpr and errors.empty() and analyzed;
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

        LOG_INFO(DOM, inferred << " vars made consts, joined " << joined << " statements to their const" << (plain ? " (plain fills)" : ""));

        return changed;
    }

    std::vector<std::string> ProgramConstsPass::takeErrors()
    {
        std::vector<std::string> taken;
        taken.swap(errors);
        return taken;
    }

    std::string ProgramConstsPass::factsAboutIdentifiers(const std::set<std::string>& identifiers) const
    {
        // What its choices for a module came to
        return analyzed ? effects.factsOf(identifiers) : "unread";
    }
}
