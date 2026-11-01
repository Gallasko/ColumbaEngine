#pragma once

/**
 * @file program_view.h
 * @brief What a whole program defines, for AST passes that must know every definition of a name.
 *
 * The program is the script being compiled and every module it imports, found as the compiler
 * finds them (beside the script: VM::currentFileName). A module with a .pgc only cannot be read:
 * `complete` is then false and a pass relying on the view should change nothing.
 */

#include "Interpreter/statement.h"

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace pg
{
    struct VM;

    namespace ast
    {
        struct ProgramView
        {
            std::map<std::string, int> defined;                                // Top-level `fun` definitions by name
            std::map<std::string, std::shared_ptr<FunctionStatement>> functions;
            std::set<std::string> poisoned;                                    // Assigned, declared or a class somewhere
            std::set<std::string> visited;                                     // Files read
            bool complete = true;

            // Every name that may hold script code: a `fun`, or anything written
            std::set<std::string> scriptNames() const
            {
                std::set<std::string> out = poisoned;

                for (const auto& [name, count] : defined)
                    out.insert(name);

                return out;
            }
        };

        ProgramView readProgram(VM* vm, const std::queue<StatementPtr>& statements);

        // By file: the statements (their index, the parser's empty ones left aside) that import a file
        // first, and which
        using FileImports = std::vector<std::vector<std::pair<size_t, size_t>>>;

        // The program's files whole: the script's statements first, then those of each module it
        // imports (once, as parsed from its source: shared, never to be changed). Not complete when
        // what runs is not what is read: a module compiled to a .pgc (the compiler takes that, source
        // or not), an import below a file's top level (it runs, and is not read)
        struct ProgramFiles
        {
            std::vector<std::vector<StatementPtr>> files;
            std::vector<std::string> origins;  // By file: its path and a digest of its source, "" with no file
            FileImports imports;
            bool complete = true;
        };

        ProgramFiles readProgramFiles(VM* vm, const std::queue<StatementPtr>& statements);

        // A statement of that kind in `s` or anywhere in the statements below it; in the bodies of
        // functions and classes only with `intoFunctions`
        bool holdsStatement(const StatementPtr& s, AstKind kind, bool intoFunctions);
    }
}
