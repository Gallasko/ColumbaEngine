#include "oneventcomponent.h"

#include "ECS/entitysystem.h"

namespace pg
{
    void OnEventComponent::onCreation(EntityRef entity)
    {
        const auto& id = eventIdCallback(entity.ecsRef->registry);

        entity.ecsRef->registry.addEventListener(entity, id, callback);
    }

    void OnEventComponent::onDeletion(EntityRef entity)
    {
        const auto& id = eventIdCallback(entity.ecsRef->registry);

        entity.ecsRef->registry.removeEventListener(entity, id);
    }

    void OnStandardEventComponent::onCreation(EntityRef entity)
    {
        entity.ecsRef->registry.addStandardEventListener(entity, eventName, callback);
    }

    void OnStandardEventComponent::onDeletion(EntityRef entity)
    {
        entity.ecsRef->registry.removeStandardEventListener(entity, eventName);
    }
}