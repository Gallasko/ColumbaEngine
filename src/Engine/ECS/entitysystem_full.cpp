/**
 * @file entitysystem_full.cpp
 * @brief Full build implementation of EntitySystem (non-minimal build features)
 *
 * This file contains implementations that are only included in non-minimal builds.
 * This avoids recompiling entitysystem.cpp with different flags.
 */

#include "stdafx.h"
#include "entitysystem.h"

// Full build modules
#include "ecsmodule.h"
#include "Helpers/randommodule.h"
#include "Helpers/inputmodule_vm.h"
#include "Input/inputcomponent.h"
#include "2D/texturemodule.h"
#include "UI/uimodule.h"

namespace pg
{
    InterpreterSystem* EntitySystem::createInterpreterSystem(std::shared_ptr<Environment> env, std::shared_ptr<ClassInstance> sysInstance)
    {
        LOG_THIS_MEMBER("ECS");

        // Todo: add support for system creation during runtime
        if (running)
        {
            LOG_ERROR("ECS", "System creation during runtime is not supported");
            return nullptr;
        }

        auto system = new InterpreterSystem(env, sysInstance);
        system->_id = registry.idGenerator.generateId();

        system->ecsRef = this;

        systems.emplace(system->_id, system);

        system->addToRegistry(&registry);

        internalCreateSystem(system);

        return system;
    }

    void EntitySystem::setupVmFullModules(VM& vm)
    {
        vm.addNativeModule("random", RandomModule{});
        vm.addNativeModule("texture", TextureModule{this});
        vm.addNativeModule("ui", UIModule{this});

        // Todo change this
        // Get the Input handler from the MouseClickSystem
        Input* inputHandler = nullptr;
        auto mouseClickSys = getSystem<MouseClickSystem>();
        if (mouseClickSys)
        {
            inputHandler = mouseClickSys->inputHandler;
        }

        // Add input module if we have an input handler
        if (inputHandler)
        {
            vm.addNativeModule("input", InputModuleVM{inputHandler});
        }
    }
}
