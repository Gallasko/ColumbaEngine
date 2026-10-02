#include "stdafx.h"

#include "componentattach.h"

#include "ECS/entitysystem.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Component Attach";
    }

    namespace detail
    {
        NativeFn createAttachCompFunction(Entity* entity, EntitySystem* ecsRef)
        {
            return [entity, ecsRef](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 1)
                    throw std::runtime_error("attachComp expects at least 1 argument (componentName)");

                if (not IS_STRING(args[0]))
                    throw std::runtime_error("attachComp expects first argument to be component name (string)");

                auto componentName = vm->asString(args[0]);

                // A component with a registered handler is built with its real C++ type, from the arguments after the name
                auto handler = ComponentAttachRegistry::instance().findHandler(componentName);

                if (handler)
                    return makeBoolValue(handler(vm, ecsRef, entity, argCount - 1, args + 1));

                // Anything else becomes a StandardComponent holding the key / value pairs as properties
                CompRef<StandardComponent> component = ecsRef->_attach(entity, componentName);

                for (int i = 1; i < argCount; i += 2)
                {
                    if (i + 1 >= argCount)
                        throw std::runtime_error("attachComp expects key-value pairs for component properties");

                    if (not IS_STRING(args[i]))
                        throw std::runtime_error("attachComp expects string keys for properties");

                    auto key = vm->asString(args[i]);
                    auto value = args[i + 1];

                    if (IS_INT(value))
                    {
                        component->properties[key] = ElementType{static_cast<int>(AS_INT(value))};
                    }
                    else if (IS_DOUBLE(value))
                    {
                        component->properties[key] = ElementType{AS_DOUBLE(value)};
                    }
                    else if (IS_STRING(value))
                    {
                        component->properties[key] = ElementType{vm->asString(value)};
                    }
                    else if (IS_BOOL(value))
                    {
                        component->properties[key] = ElementType{AS_BOOL(value)};
                    }
                    else
                    {
                        throw std::runtime_error("attachComp: unsupported value type for property '" + key + "'");
                    }
                }

                LOG_INFO(DOM, "Attached StandardComponent '" << componentName << "' to entity " << entity->id);

                return makeBoolValue(true);
            };
        }
    }
}
