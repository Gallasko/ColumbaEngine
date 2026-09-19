/**
 * @file entitysystem_minimal.cpp
 * @brief Minimal build implementation of EntitySystem
 *
 * This file contains stub implementations for minimal builds (bootstrap compiler).
 * This avoids recompiling entitysystem.cpp with different flags.
 */

#include "stdafx.h"
#include "entitysystem.h"

namespace pg
{
    InterpreterSystem* EntitySystem::createInterpreterSystem(std::shared_ptr<Environment>, std::shared_ptr<ClassInstance>)
    {
        return nullptr;
    }

    void EntitySystem::setupVmFullModules(VM&)
    {
        // No additional modules in minimal build
    }

    void EntitySystem::enableCollision()
    {
        LOG_ERROR("ECS", "enableCollision is not available in minimal builds");
    }
}
