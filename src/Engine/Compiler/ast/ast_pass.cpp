#include "stdafx.h"

#include "ast_pass.h"

#include "logger.h"

#include <iostream>

namespace pg
{
    namespace
    {
        const char* DOM = "AstPassManager";
    }

    void AstPassManager::addPass(std::unique_ptr<AstPass> pass)
    {
        passes.push_back(std::move(pass));
    }

    void AstPassManager::runAllPasses(VM* vm, std::queue<StatementPtr>& statements)
    {
        for (auto& pass : passes)
        {
            debugPrint("Running AST pass: " + pass->getName());

            bool changed = pass->runPass(vm, statements);

            if (changed)
            {
                LOG_INFO(DOM, "AST pass '" << pass->getName() << "' transformed the program");
            }
        }
    }

    void AstPassManager::listPasses() const
    {
        std::string message = "Registered AST passes (" + std::to_string(passes.size()) + "):";

        for (const auto& pass : passes)
        {
            message += "\n  - " + pass->getName();
        }

        debugPrint(message);
    }

    void AstPassManager::clearPasses()
    {
        passes.clear();
    }

    std::string AstPassManager::passNames() const
    {
        std::string names;

        for (const auto& pass : passes)
            names += pass->getName() + ";";

        return names;
    }

    std::string AstPassManager::moduleFacts(std::set<std::string> names, std::vector<std::shared_ptr<const void>>& keep) const
    {
        // The names brought bring names in turn, until no pass brings more
        size_t before = 0;

        while (names.size() != before)
        {
            before = names.size();

            for (const auto& pass : passes)
                pass->namesBrought(names);
        }

        std::string facts;

        for (const auto& pass : passes)
            facts += pass->getName() + "{" + pass->factsAbout(names, keep) + "}";

        return facts;
    }

    void AstPassManager::debugPrint(const std::string& message) const
    {
        if (enableDebugOutput)
        {
            LOG_INFO(DOM, message);
        }
    }
}
