#pragma once

#include "entitysystem.h"
#include "Compiler/vm.h"

namespace pg
{
    // Template implementation for registerCustomVmModule
    // This must be included in .cpp files that call registerCustomVmModule
    template<typename ModuleType>
    void EntitySystem::registerCustomVmModule(const std::string& name, ModuleType&& module)
    {
        addCustomVmModuleRegistrar([name, module](VM& vm) {
            vm.addNativeModule(name, module);
        });
    }
}