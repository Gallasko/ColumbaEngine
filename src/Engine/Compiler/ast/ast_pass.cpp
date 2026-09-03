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
        std::cout << "Registered AST passes (" << passes.size() << "):" << std::endl;

        for (const auto& pass : passes)
        {
            std::cout << "  - " << pass->getName() << std::endl;
        }
    }

    void AstPassManager::clearPasses()
    {
        passes.clear();
    }

    void AstPassManager::debugPrint(const std::string& message) const
    {
        if (enableDebugOutput)
        {
            std::cout << "[AstPassManager] " << message << std::endl;
        }
    }
}
