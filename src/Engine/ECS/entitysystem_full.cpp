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
#include "Helpers/randommodule.h"
#include "Helpers/inputmodule_vm.h"
#include "Input/inputcomponent.h"
#include "2D/collisionsystem.h"
#include "2D/texturemodule.h"
#include "UI/uimodule.h"

namespace pg
{
    void EntitySystem::enableCollision()
    {
        if (getSystem<CollisionSystem>())
        {
            LOG_WARNING("ECS", "enableCollision called but collision is already enabled");
            return;
        }

        createSystem<CollisionSystem>();
        createSystem<CollisionHandlerSystem>();

        succeed<CollisionHandlerSystem, CollisionSystem>();
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
