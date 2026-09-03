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

    void AstPassManager::debugPrint(const std::string& message) const
    {
        if (enableDebugOutput)
        {
            LOG_INFO(DOM, message);
        }
    }
}
