#include "interpretersystem.h"

#include "scriptcallable.h"

namespace pg
{
    void InterpreterSystem::addToRegistry(ComponentRegistry *registry) 
    {
        LOG_THIS_MEMBER("Interpreter System");

        this->registry = registry;

        this->ecsRef = registry->world();

        auto sysMethods = sysInstance->getMethods();
        auto sysFields = sysInstance->getFields();

        const auto& nameIt = std::find(sysFields.begin(), sysFields.end(), "name");

        if (nameIt != sysFields.end())
            this->name = nameIt->value->getValue()->getElement().toString();

        const auto& policyIt = std::find(sysFields.begin(), sysFields.end(), "policy");

        if (policyIt != sysFields.end())
        {
            const auto policy = policyIt->value->getValue()->getElement().toString();

            if (policy == "manual")
            {
                this->executionPolicy = ExecutionPolicy::Manual;
            }
            else if (policy == "sequential")
            {
                this->executionPolicy = ExecutionPolicy::Sequential;
            }
            else if (policy == "parallel")
            {
                this->executionPolicy = ExecutionPolicy::Parallel;
            }
            else if (policy == "independent")
            {
                this->executionPolicy = ExecutionPolicy::Independent;
            }
            else if (policy == "storage")
            {
                this->executionPolicy = ExecutionPolicy::Storage;
            }
        }

        if (executionPolicy == ExecutionPolicy::Independent or executionPolicy == ExecutionPolicy::Sequential)
        {
            // Todo also check for a sysfield name "execute" !
            const auto& executeIt = sysMethods.find("execute");

            if (executeIt != sysMethods.end())
            {
                // Todo check for the arity of the function when registering
                if (executeIt->second->getType() == "Function")
                {
                    executeMethod = makeCallable(std::static_pointer_cast<Function>(executeIt->second));
                }
                else
                {
                    throw RuntimeException(Token{TokenType::EXPRESSION, executeIt->second->getElement().toString(), 0, 0}, "Method execute is not a function for an executable system");
                }
            }
            else
            {
                throw RuntimeException(Token{TokenType::EXPRESSION, sysInstance->getElement().toString(), 0, 0}, "No execute method found for an executable system");
            }
        }
    }

    void InterpreterSystem::execute()
    {
        if (not executeMethod)
            return;

        ValuableQueue emptyQueue;
        
        try
        {    
            executeMethod->call(emptyQueue);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Interpreter System", e.what());
        }

    }
}