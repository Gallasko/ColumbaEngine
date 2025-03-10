#pragma once

#include "entitysystem.h"

#include "serialization.h"

namespace pg
{
    /**
     * @class AbstractCallable
     * 
     * An abstract struct that holds all the data to make a callable function for any system
     */
    struct AbstractCallable
    {
        virtual ~AbstractCallable() {}

        /**
         * @brief Virtual call function, need to be implemented by derived classes
         * 
         * @param ecsRef Reference to the ecs to send events or manipulate entities
         */
        virtual void call(EntitySystem* const ecsRef) noexcept = 0;

        virtual void serialize(Archive& archive) const noexcept = 0;
    };

    typedef std::shared_ptr<AbstractCallable> CallablePtr;

    /**
     * @brief Redefinition of AbstractCallable that sends events to the ecs when an action is executed
     * 
     * @tparam Event The type of event to send
     * @tparam Types The type of the args of the event
     */
    template <typename Event, typename... Types>
    struct CallableEvent : public AbstractCallable
    {
        /**
         * @brief Construct a new Callable Event object
         * 
         * @param args All the arguments to generate the event that will be send when call is invoked
         */
        CallableEvent(Types&&... args) : event(std::forward<Types>(args)...) {}

        virtual ~CallableEvent() {}

        /**
         * @brief Send the created event to the ECS
         * 
         * @param ecsRef A reference to an ECS
         */
        inline virtual void call(EntitySystem* const ecsRef) noexcept override { if (ecsRef) ecsRef->sendEvent(event); }

        inline virtual void serialize(Archive& archive) const noexcept override
        {
            archive.startSerialization("CallableEvent");

            pg::serialize(archive, "event", event);

            archive.endSerialization();
        }

        /** The event to send */
        Event event;
    };

    /**
     * @brief Helper function that create a callable pointer
     * 
     * @tparam Event The type of event to send
     * @tparam Types The type of the args of the event
     * @param args The actual args of the event
     * 
     * @return std::shared_ptr<CallableEvent<Event, Types...>> A pointer to the event callable
     */
    template <typename Event, typename... Types>
    inline std::shared_ptr<CallableEvent<Event, Types...>> makeCallable(Types&&... args) { return std::make_shared<CallableEvent<Event, Types...>>(std::forward<Types>(args)...); }
}