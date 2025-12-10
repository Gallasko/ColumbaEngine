#pragma once

#include "entitysystem.h"

#include "Interpreter/pginterpreter.h"
#include "Interpreter/interpretersystem.h"

#include "Compiler/native_module.h"
#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"

namespace pg
{
    class GetAllSystemFunction : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            setArity(0, 0);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            LOG_THIS_MEMBER("Ecs Module");

            auto systemList = makeList(this, {});

            for (auto sys : ecsRef->getSystems())
            {
                addToList(systemList, this->token, {std::to_string(sys.first), sys.second->getSystemName()});
            }

            return systemList;
        }

        EntitySystem *ecsRef;
    };

    class GetAllEntityFunction : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            setArity(0, 0);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            LOG_THIS_MEMBER("Ecs Module");

            auto entityList = makeList(this, {});

            for (auto entity : ecsRef->view())
            {
                auto compList = makeList(this, {});

                size_t j = 0;
                for (const auto& compId : entity->componentList)
                {
                    addToList(compList, this->token, {std::to_string(j), compId.getId()});
                    j++;
                }

                addToList(entityList, this->token, {std::to_string(entity->id), compList});
            }

            return entityList;
        }

        EntitySystem *ecsRef;
    };

    class GetNameFromCompFunction : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            setArity(1, 1);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Ecs Module");

            auto arg = args.front()->getElement();
            args.pop();

            if (not arg.isNumber())
            {
                LOG_ERROR("Ecs Module", "Id given is not a number");
                return makeVar(arg);
            }

            ecsRef->removeEntity(ecsRef->getEntity(arg.get<size_t>()));

            return nullptr;
        }

        EntitySystem *ecsRef;
    };

    class RegisterNewSystem : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            setArity(1, 1);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Ecs Module");

            auto arg = args.front();
            args.pop();

            if (arg->getType() == "ClassInstance")
            {
                auto sys = std::static_pointer_cast<ClassInstance>(arg);

                // Todo enable sys creation during runtime
                if (not ecsRef->isRunning())
                {
                    ecsRef->createInterpreterSystem(env, sys);
                    visitor->setEcsSysFlag();
                }
                else
                {
                    LOG_ERROR("Ecs Module", "Trying to instanciate a system while ecs is running");
                }
            }

            return nullptr;
        }

        EntitySystem *ecsRef;
    };

    class NewUniqueId : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            setArity(0, 0);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            LOG_THIS_MEMBER("Ecs Module");

            return makeVar(ecsRef->generateId());
        }

        EntitySystem *ecsRef;
    };

    class DeleteEntityFromId : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            setArity(1, 1);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Ecs Module");

            auto arg = args.front()->getElement();
            args.pop();

            if (not arg.isNumber())
            {
                LOG_ERROR("Ecs Module", "Id given is not a number");
                return nullptr;
            }

            ecsRef->removeEntity(ecsRef->getEntity(arg.get<size_t>()));

            return nullptr;
        }

        EntitySystem *ecsRef;
    };

    class NewUniqueIdFromString : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            setArity(1, 1);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Ecs Module");

            auto arg = args.front();
            args.pop();

            auto key = arg->getValue()->getElement().toString();

            const auto& it = uniqueIds.find(key);

            if (it != uniqueIds.end())
                return makeVar(it->second);
            else
            {
                auto id = ecsRef->generateId();
                uniqueIds.emplace(key, id);
                return makeVar(id);
            }
        }

        EntitySystem *ecsRef;
        mutable std::unordered_map<std::string, _unique_id> uniqueIds;
    };

    struct EcsModule : public SysModule
    {
        EcsModule(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Module");

            addSystemFunction<GetAllSystemFunction>("getAllSystems", ecsRef);
            addSystemFunction<GetAllEntityFunction>("getAllEntities", ecsRef);
            addSystemFunction<RegisterNewSystem>("registerSystem", ecsRef);
            addSystemFunction<NewUniqueId>("generateNewId", ecsRef);
            addSystemFunction<NewUniqueIdFromString>("getIdFrom", ecsRef);
            addSystemFunction<DeleteEntityFromId>("deleteEntityFromId", ecsRef);
        }
    };

    /**
     * @brief ECS Module for compiled scripts (VM-based)
     *
     * Provides native functions for working with the ECS from scripts:
     *
     * - removeEntity(entityId or entityInstance): Remove an entity from the ECS
     * - getEntity(entityId): Get entity data as a table
     * - sendEvent(eventName, key1, value1, key2, value2, ...): Send an event
     * - attachComponent(entityId or entityInstance, componentName, prop1, value1, prop2, value2, ...):
     *   Attach a StandardComponent to an entity with properties
     *
     * Example usage:
     * @code
     * import ecs
     *
     * // Get an entity
     * var player = ecs.getEntity(playerId)
     *
     * // Attach a Health component with initial values
     * ecs.attachComponent(player, "Health", "current", 100, "max", 100)
     *
     * // Attach a Position component
     * ecs.attachComponent(playerId, "Position", "x", 10.0, "y", 20.0)
     *
     * // Send an event
     * ecs.sendEvent("PlayerDamaged", "entityId", playerId, "damage", 25)
     * @endcode
     */
    struct EcsCompiledModule : public NativeModule
    {
        EntitySystem *ecsRef = nullptr;

        EcsCompiledModule(EntitySystem *ecsRef) : ecsRef(ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Compiled Module");

            // Capture ecsRef by value instead of capturing 'this' to avoid dangling pointer
            auto ecsRefCopy = ecsRef;
            addNativeFunction("removeEntity", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    throw std::runtime_error("removeEntity expects exactly 1 argument");
                }

                if (IS_INT(args[0]))
                {
                    ecsRefCopy->removeEntity(AS_INT(args[0]));
                    return args[0];
                }
                else if (IS_INSTANCE(args[0]))
                {
                    auto instance = vm->asInstance(args[0]);

                    if (instance->fields.find("__entityId") == instance->fields.end())
                    {
                        throw std::runtime_error("removeEntity expects an entity with an __entityId field");
                    }

                    auto idValue = instance->fields.at("__entityId");

                    if (not IS_INT(idValue))
                    {
                        throw std::runtime_error("removeEntity expects an entity with an integer __entityId field");
                    }

                    auto entityId = AS_INT(idValue);

                    LOG_INFO("Ecs Compiled Module", "Removing entity with id " << entityId);

                    ecsRefCopy->removeEntity(entityId);
                    return args[0];
                }

                throw std::runtime_error("removeEntity expects an integer id or an entity instance");
            });

            addNativeFunction("getEntity", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    throw std::runtime_error("getEntity expects exactly 1 argument");
                }

                if (!IS_INT(args[0]))
                {
                    throw std::runtime_error("getEntity expects an integer id");
                }

                auto entityId = AS_INT(args[0]);
                auto entity = ecsRefCopy->getEntity(entityId);

                if (!entity)
                {
                    throw std::runtime_error("Entity with id " + std::to_string(entityId) + " not found");
                }

                return serializeEntityToTable(vm, ecsRefCopy, entity);
            });

            addNativeFunction("sendEvent", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 1)
                {
                    throw std::runtime_error("sendEvent expects at least 1 argument (event name)");
                }

                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("sendEvent expects first argument to be a string (event name)");
                }

                auto eventName = vm->asString(args[0])->toString();
                StandardEvent event(eventName);

                // Process remaining arguments as key-value pairs
                for (int i = 1; i < argCount; i += 2)
                {
                    if (i + 1 >= argCount)
                    {
                        throw std::runtime_error("sendEvent expects key-value pairs after event name");
                    }

                    if (!IS_STRING(args[i]))
                    {
                        throw std::runtime_error("sendEvent expects string keys");
                    }

                    auto key = vm->asString(args[i])->toString();
                    auto value = args[i + 1];

                    // Convert Value to ElementType
                    if (IS_INT(value))
                    {
                        event.values[key] = ElementType{static_cast<int>(AS_INT(value))};
                    }
                    else if (IS_DOUBLE(value))
                    {
                        event.values[key] = ElementType{AS_DOUBLE(value)};
                    }
                    else if (IS_STRING(value))
                    {
                        event.values[key] = ElementType{vm->asString(value)->toString()};
                    }
                    else if (IS_BOOL(value))
                    {
                        event.values[key] = ElementType{AS_BOOL(value)};
                    }
                    else
                    {
                        throw std::runtime_error("sendEvent: unsupported value type for key " + key);
                    }
                }

                LOG_INFO("Ecs Compiled Module", "Sending event: " << eventName);
                ecsRefCopy->sendEvent(event);

                return makeIntValue(0);
            });

            addNativeFunction("attachComp", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 2)
                {
                    throw std::runtime_error("attachComponent expects at least 2 arguments (entityId, componentName)");
                }

                // Get entity ID (can be either int or entity table)
                _unique_id entityId;
                if (IS_INT(args[0]))
                {
                    entityId = AS_INT(args[0]);
                }
                else if (IS_INSTANCE(args[0]))
                {
                    auto instance = vm->asInstance(args[0]);
                    if (instance->fields.find("__entityId") == instance->fields.end())
                    {
                        throw std::runtime_error("attachComponent: entity instance must have __entityId field");
                    }

                    auto idValue = instance->fields.at("__entityId");
                    if (!IS_INT(idValue))
                    {
                        throw std::runtime_error("attachComponent: __entityId must be an integer");
                    }
                    entityId = AS_INT(idValue);
                }
                else
                {
                    throw std::runtime_error("attachComponent expects first argument to be entity ID (int) or entity instance");
                }

                // Get component name
                if (!IS_STRING(args[1]))
                {
                    throw std::runtime_error("attachComponent expects second argument to be component name (string)");
                }
                auto componentName = vm->asString(args[1])->toString();

                // Get the entity
                auto entity = ecsRefCopy->getEntity(entityId);
                if (!entity)
                {
                    throw std::runtime_error("attachComponent: entity with ID " + std::to_string(entityId) + " not found");
                }

                // Build the properties for the StandardComponent from key-value pairs
                std::vector<std::pair<std::string, ElementType>> properties;

                // Process remaining arguments as key-value pairs for component properties
                for (int i = 2; i < argCount; i += 2)
                {
                    if (i + 1 >= argCount)
                    {
                        throw std::runtime_error("attachComponent expects key-value pairs for component properties");
                    }

                    if (!IS_STRING(args[i]))
                    {
                        throw std::runtime_error("attachComponent expects string keys for properties");
                    }

                    auto key = vm->asString(args[i])->toString();
                    auto value = args[i + 1];

                    // Convert Value to ElementType
                    ElementType elementValue;
                    if (IS_INT(value))
                    {
                        elementValue = ElementType{static_cast<int>(AS_INT(value))};
                    }
                    else if (IS_DOUBLE(value))
                    {
                        elementValue = ElementType{AS_DOUBLE(value)};
                    }
                    else if (IS_STRING(value))
                    {
                        elementValue = ElementType{vm->asString(value)->toString()};
                    }
                    else if (IS_BOOL(value))
                    {
                        elementValue = ElementType{AS_BOOL(value)};
                    }
                    else
                    {
                        throw std::runtime_error("attachComponent: unsupported value type for property '" + key + "'");
                    }

                    properties.push_back({key, elementValue});
                }

                // Attach the StandardComponent with properties
                try
                {
                    // Create the component with the first property if it exists
                    CompRef<StandardComponent> component;
                    if (properties.empty())
                    {
                        component = ecsRefCopy->attach(entity, componentName);
                    }
                    else
                    {
                        component = ecsRefCopy->attach(entity, componentName,
                            properties[0].first, properties[0].second);

                        // Add remaining properties
                        for (size_t i = 1; i < properties.size(); i++)
                        {
                            component->properties[properties[i].first] = properties[i].second;
                        }
                    }

                    LOG_INFO("Ecs Compiled Module", "Attached StandardComponent '" << componentName
                             << "' to entity " << entityId << " with " << properties.size() << " properties");

                    return makeBoolValue(true);
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error("attachComponent: failed to attach component '" + componentName + "': " + e.what());
                }
            });

        }
    };

}