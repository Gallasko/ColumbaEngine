#pragma once

#include <string>
#include <unordered_map>

#include "ECS/entitysystem_fwd.h"

#include "logger.h"

#include "vm.h"

namespace pg
{
    /**
     * @brief Handler that attaches one component type to an entity from script arguments
     *
     * argCount and args hold what the script passed to attachComp() after the component name, as key / value pairs.
     *
     * @return true if the component was attached
     */
    using ComponentAttachFunc = bool (*)(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args);

    /**
     * @brief Registry of the components that attachComp() can build with their real C++ type
     *
     * A component name without a handler is attached as a StandardComponent.
     */
    class ComponentAttachRegistry
    {
    public:
        static ComponentAttachRegistry& instance()
        {
            static ComponentAttachRegistry registry;

            return registry;
        }

        void registerHandler(const std::string& componentName, ComponentAttachFunc handler)
        {
            handlers[componentName] = handler;
        }

        /** @return The handler of the component, or nullptr if it has none */
        ComponentAttachFunc findHandler(const std::string& componentName) const
        {
            auto it = handlers.find(componentName);

            if (it == handlers.end())
                return nullptr;

            return it->second;
        }

    private:
        std::unordered_map<std::string, ComponentAttachFunc> handlers;
    };

    namespace detail
    {
        // Argument readers for the attach handlers. The caller checks that index is within the arguments
        inline float extractFloatArg(Value* args, int index = 0)
        {
            if (IS_DOUBLE(args[index]))
                return static_cast<float>(AS_DOUBLE(args[index]));

            if (IS_INT(args[index]))
                return static_cast<float>(AS_INT(args[index]));

            LOG_ERROR("Component Attach", "Expected float argument at index " << index);

            return 0.0f;
        }

        inline bool extractBoolArg(Value* args, int index = 0)
        {
            if (IS_BOOL(args[index]))
                return AS_BOOL(args[index]);

            LOG_ERROR("Component Attach", "Expected bool argument at index " << index);

            return false;
        }

        inline int extractIntArg(Value* args, int index = 0)
        {
            if (IS_INT(args[index]))
                return static_cast<int>(AS_INT(args[index]));

            LOG_ERROR("Component Attach", "Expected int argument at index " << index);

            return 0;
        }

        inline std::string extractStringArg(VM *vm, Value* args, int index = 0)
        {
            if (IS_STRING(args[index]))
                return vm->asString(args[index]);

            LOG_ERROR("Component Attach", "Expected string argument at index " << index);

            return "";
        }

        /**
         * @brief Create the attachComp(name, key, value, ...) native bound to one entity
         *
         * @param entity Entity the components get attached to
         * @param ecsRef Entity system owning the entity
         */
        NativeFn createAttachCompFunction(Entity* entity, EntitySystem* ecsRef);
    }

    /**
     * @brief Register the attach handler of a component
     *
     * The handler is called when a script runs attachComp("ComponentName", ...).
     *
     * @code
     * bool attachPositionComponent(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
     * {
     *     float x = 0.0f;
     *
     *     for (int i = 0; i + 1 < argCount; i += 2)
     *     {
     *         if (vm->asString(args[i]) == "x")
     *             x = detail::extractFloatArg(args, i + 1);
     *     }
     *
     *     ecs->_attach<PositionComponent>(entity)->setX(x);
     *
     *     return true;
     * }
     *
     * REGISTER_COMPONENT_ATTACH_HANDLER(Position, attachPositionComponent);
     * @endcode
     */
    #define REGISTER_COMPONENT_ATTACH_HANDLER(ComponentName, HandlerFunc) \
        namespace { \
            struct ComponentName##AttachRegistrar { \
                ComponentName##AttachRegistrar() { \
                    pg::ComponentAttachRegistry::instance() \
                        .registerHandler(#ComponentName, HandlerFunc); \
                } \
            }; \
            static ComponentName##AttachRegistrar ComponentName##_attach_registrar_instance; \
        }
}
