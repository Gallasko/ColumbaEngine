#pragma once

#include "ECS/system.h"

namespace pg
{
    struct OnEventComponent : public Ctor, public Dtor
    {
        template <typename Event>
        OnEventComponent(const std::function<void(const Event&)>& eventCallback)
        {
            eventIdCallback = [](const ComponentRegistry& registry) -> _unique_id {
                return registry.getTypeId<Event>();
            };

            callback = [eventCallback](const std::any& event) {
                eventCallback(std::any_cast<const Event&>(event));
            };
        }

        template <typename Event>
        OnEventComponent(void(*eventCallback)(const Event&))
        {
            eventIdCallback = [](const ComponentRegistry& registry) -> _unique_id {
                return registry.getTypeId<Event>();
            };

            callback = [eventCallback](const std::any& event) {
                eventCallback(std::any_cast<const Event&>(event));
            };
        }

        OnEventComponent(const OnEventComponent& other) : eventIdCallback(other.eventIdCallback), callback(other.callback)
        {

        }

        virtual void onCreation(EntityRef entity) override;

        virtual void onDeletion(EntityRef entity) override;

        std::function<_unique_id(const ComponentRegistry& registry)> eventIdCallback;

        std::function<void(const std::any&)> callback;
    };

    struct OnStandardEventComponent : public Ctor, public Dtor
    {
        OnStandardEventComponent(const std::string& eventName, const std::function<void(const StandardEvent&)>& eventCallback) : eventName(eventName), callback(eventCallback)
        {
        }

        OnStandardEventComponent(const std::string& eventName, void(*eventCallback)(const StandardEvent&)) : eventName(eventName), callback(eventCallback)
        {
        }

        OnStandardEventComponent(const OnStandardEventComponent& other) : eventName(other.eventName), callback(other.callback)
        {
        }

        virtual void onCreation(EntityRef entity) override;

        virtual void onDeletion(EntityRef entity) override;

        std::string eventName;

        std::function<void(const StandardEvent&)> callback;
    };

    struct OnEventComponentSystem : public System<Own<OnEventComponent>, Own<OnStandardEventComponent>, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "OnEventComponentSystem"; }
    };
}