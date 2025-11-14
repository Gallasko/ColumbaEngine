#pragma once

#include "entitysystem.h"

#include "Interpreter/pginterpreter.h"
#include "Interpreter/interpretersystem.h"

#include "Compiler/native_module.h"
#include "Compiler/vm.h"

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
        }

        Value removeEntity(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("removeEntity expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                ecsRef->removeEntity(AS_INT(args[0]));
                return args[0]; // Already an integer
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

                ecsRef->removeEntity(entityId);
                return args[0];
            }

            throw std::runtime_error("removeEntity expects an integer id or an entity instance");
        }
    };

}