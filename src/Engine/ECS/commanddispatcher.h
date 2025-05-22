#pragma once

#include "entity.h"

#include "Memory/concurrentqueue.h"

#include "logger.h"

namespace pg
{
    // Type forwarding
    class EntitySystem;
    class Entity;

    /**
     * @brief Structure holding the different types of system commands that the dispatcher can handle
     */
    struct SysCommand
    {
        /**
         * @brief Enum of all the different dispatchable system commands
         */
        enum class SysCommandType
        {
            createEntity, ///< Dispatch the creation of a new entity
            deleteEntity, ///< Dispatch the deletion of a entity
            startSystem,  ///< Dispatch the starting of a system
            stopSystem,   ///< Dispatch the stopping of a system
            log           ///< Dispatch a blocking log message
        };

        /** Type of the command */
        SysCommandType type;
    };

    /**
     * @brief A base ECS system responsible of system commands generated from other systems.
     *
     * This system is responsible for creating and running ECS system commands
     * Should be used for every commands that need to be executed asynchronously such as
     * creating or deleting an Entity or a component.
     */
    class CommandDispatcher
    {
    public:
        typedef moodycamel::ConcurrentQueue<SysCommand>::producer_token_t CommandToken;

        /**
         * @brief Structure holding the different types of entity commands that the dispatcher can handle
         */
        struct EntityCommand
        {
            /**
             * @brief Enum of all the different dispatchable entity commands
             */
            enum class EntityCommandType
            {
                creation = 0,
                deletion = 1
            };

            EntityCommand(Entity *entity, const EntityCommandType& type) : entity(entity), type(type) {}

            /** Underlaying entity of this command */
            Entity *entity;
            /** Type of action to execute in the dispatcher */
            EntityCommandType type;
        };

        /**
         * @brief Structure holding the different types of components commands that the dispatcher can handle
         */
        struct ComponentCreateCommand
        {
            ComponentCreateCommand() : component(nullptr) {}

            ComponentCreateCommand(const ComponentCreateCommand& other) : entity(other.entity), component(other.component), addInEcs(other.addInEcs) {}

            template <typename Type>
            ComponentCreateCommand(EntityRef entity, Type *component) : entity(entity)
            {
                this->component = component;

                setupFunctions<Type>();
            }

            void operator=(const ComponentCreateCommand& other)
            {
                entity = other.entity;
                component = other.component;
                addInEcs = other.addInEcs;
            }

            template <typename Type>
            void setupFunctions();

            EntityRef entity;
            void *component;

            void(*addInEcs)(EntitySystem*, EntityRef, void*);
        };

        struct ComponentDeleteCommand
        {
            ComponentDeleteCommand() {}

            ComponentDeleteCommand(const ComponentDeleteCommand& other) : entity(other.entity), compId(other.compId) {}

            ComponentDeleteCommand(Entity* entity, _unique_id compId) : entity(entity), compId(compId)
            {
            }

            void operator=(const ComponentDeleteCommand& other)
            {
                entity = other.entity;
                compId = other.compId;
            }

            Entity* entity;
            _unique_id compId;
        };

    public:
        CommandDispatcher(EntitySystem *ecs) : ecsRef(ecs) { LOG_THIS_MEMBER("Command Dispatcher"); }

        /** Enqueue the creation of a new entity */
        EntityRef createEntity();

        /** Enqueue the deletion of an entity */
        void deleteEntity(Entity* entity);

        /**
         * @brief Attach a new component to an entity
         *
         * @tparam Type Type of the component to be attached
         * @tparam Args Type of the arguments of the component to be attached
         * @param entity Entity where the component will be attached
         * @param args Arguments used to create the component to be attached
         * @return Type* A pointer to the newly attached component
         */
        template <typename Type, typename... Args>
        Type* attachComp(EntityRef entity, Args&&... args)
        {
            LOG_THIS_MEMBER("Command Dispatcher");

            Type* comp = new Type(std::forward<Args>(args)...);

            if (not componentCQueue.enqueue(ComponentCreateCommand{entity, comp}))
            {
                LOG_ERROR("Command Dispatcher", "Could not enqueue the creation of component " << typeid(Type).name());
                return nullptr;
            }

            return comp;
        }

        /**
         * @brief Detach a component from an entity
         *
         * @param entity Entity of the component to be detached
         * @param compid Id of the component to be detached
         */
        void detachComp(Entity* entity, _unique_id compId)
        {
            LOG_THIS_MEMBER("Command Dispatcher");

            if (not componentDQueue.enqueue(ComponentDeleteCommand{entity, compId}))
            {
                LOG_ERROR("Command Dispatcher", "Could not enqueue the deletion of component " << compId);
            }
        }

        /**
         * @brief Enqueue a new system command in the dispatcher
         *
         * @param cmd Command to execute
         * @return true if the command was successfully enqueued, false otherwise
         */
        inline bool enqueueCommand(const SysCommand& cmd)
        {
            return sysQueue.enqueue(cmd);
        }

        /**
         * @brief Enqueue a new system command in the dispatcher
         *
         * Overload to support universal references
         *
         * @param cmd Command to execute
         * @return true if the command was successfully enqueued, false otherwise
         */
        inline bool enqueueCommand(SysCommand&& cmd)
        {
            return sysQueue.enqueue(cmd);
        }

         /**
         * @brief Enqueue a new system command in the dispatcher
         *
         * Overload to enforce a selected queue through a Command Token
         *
         * @param token Token of the queue to use
         * @param cmd Command to execute
         * @return true if the command was successfully enqueued, false otherwise
         */
        inline bool enqueueCommand(const CommandToken& token, const SysCommand& cmd)
        {
            return sysQueue.enqueue(token, cmd);
        }

        /** Process all the pending commands */
        void process();

    private:
        /** Pointer to the entity system */
        EntitySystem *const ecsRef;

        /** Queue for the entity creation commands */
        moodycamel::ConcurrentQueue<EntityCommand> entityCQueue;

        /** Queue for the entity deletion commands */
        moodycamel::ConcurrentQueue<EntityCommand> entityDQueue;

        /** Queue for the component creation commands */
        moodycamel::ConcurrentQueue<ComponentCreateCommand> componentCQueue;

        /** Queue for the component deletion commands */
        moodycamel::ConcurrentQueue<ComponentDeleteCommand> componentDQueue;

        /** Queue for the system commands */
        moodycamel::ConcurrentQueue<SysCommand> sysQueue;
    };
}