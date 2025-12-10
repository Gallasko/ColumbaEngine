#pragma once

#include "Compiler/native_module.h"
#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"
#include "ECS/system.h"

namespace pg
{
    /**
     * @brief System Module for scripts running in StandardSystemImpl context
     *
     * This module is available in onDelta and onExecute scripts through `import sys`.
     * It provides access to entities that have components owned by the current system.
     *
     * Functions:
     * - getEntities(componentName): Returns a vector of complete entity tables (with ALL components)
     *   for entities that have the specified component. Each entity table includes __entityId and
     *   all components attached to that entity as nested tables.
     *
     * Example usage in a system script:
     * @code
     * import sys
     *
     * // In a system that owns "Bullet" component
     * var bullets = sys.getEntities("Bullet")
     *
     * for (var bullet : bullets) {
     *     __dprint(bullet.__entityId)
     *
     *     // Access any component on the entity
     *     var pos = bullet["PositionComponent"]
     *     pos.setX(pos.x + 10)  // Modify PositionComponent
     *
     *     var bulletComp = bullet["Bullet"]
     *     bulletComp.lifetime = bulletComp.lifetime - deltaTime
     * }
     * @endcode
     */
    struct SystemModule : public NativeModule
    {
        StandardSystemImpl* systemImpl = nullptr;

        SystemModule(StandardSystemImpl* systemImpl) : systemImpl(systemImpl)
        {
            LOG_THIS_MEMBER("Sys Module");

            // Capture systemImpl by value to avoid dangling pointer
            auto systemImplCopy = systemImpl;

            addNativeFunction("getEntities", [systemImplCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    throw std::runtime_error("sys.getEntities expects exactly 1 argument (componentName)");
                }

                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("sys.getEntities expects a string component name");
                }

                auto componentName = vm->asString(args[0])->toString();

                // Get the component owner for this component type
                auto* owner = systemImplCopy->getComponentOwner(componentName);
                if (!owner)
                {
                    throw std::runtime_error("sys.getEntities: Component '" + componentName +
                                           "' is not owned by this system. Make sure to use .ownComponent(\"" +
                                           componentName + "\") in system builder.");
                }

                // Create a vector to hold all entity tables
                Value vectorValue = vm->createVector();
                ObjVector* vector = vm->asVector(vectorValue);

                // Get all components of this type from the owner
                auto componentView = owner->view();
                auto* ecsRef = systemImplCopy->world();

                for (auto* component : componentView)
                {
                    // Get the full entity with ALL components, not just the requested one
                    auto* entity = ecsRef->getEntity(component->entityId);
                    if (entity)
                    {
                        // Serialize the complete entity with all its components
                        Value entityTable = serializeEntityToTable(vm, ecsRef, entity);
                        vector->fields.push_back(vm->retainValue(entityTable));
                    }
                }

                LOG_MILE("Sys Module", "getEntities(\"" << componentName << "\") returned "
                         << vector->fields.size() << " entities with all their components");

                return vectorValue;
            });
        }
    };
}
