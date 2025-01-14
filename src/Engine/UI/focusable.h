#pragma once

#include "ECS/entitysystem.h"

namespace pg
{
    struct OnFocus
    {
        OnFocus(_unique_id id) : id(id) {}
        OnFocus(const OnFocus& rhs) : id(rhs.id) {}
        ~OnFocus() {}

        _unique_id id;
    };

    struct FocusableComponent : public Ctor
    {
        FocusableComponent(bool alwaysFocus = false) : alwaysFocus(alwaysFocus), focused(alwaysFocus) {}
        FocusableComponent(const FocusableComponent& rhs) : entityId(rhs.entityId), ecsRef(rhs.ecsRef), alwaysFocus(rhs.alwaysFocus), focused(rhs.alwaysFocus) {}
        ~FocusableComponent() {}

        inline static std::string getType() { return "FocusableComponent"; } 

        virtual void onCreation(EntityRef entity) override
        {
            ecsRef = entity->world();

            entityId = entity->id;
        }

        void focus()
        {
            if (ecsRef)
            {
                ecsRef->sendEvent(OnFocus{entityId});
            }
        }

        _unique_id entityId = 0;

        EntitySystem *ecsRef = nullptr;

        bool alwaysFocus;
        bool focused;
    };

    template<>
    void serialize(Archive& archive, const FocusableComponent& component);

    template<>
    FocusableComponent deserialize(const UnserializedObject& serializedString);

    struct FocusableSystem : public System<Own<FocusableComponent>, Listener<OnFocus>, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Focusable System"; }

        virtual void onEvent(const OnFocus& event) override
        {
            if (event.id == currentFocus)
            {
                return;
            }

            currentFocus = event.id;

            LOG_INFO("Focusable System", "New focus on entity: " << currentFocus);

            for (const auto& component : view<FocusableComponent>())
            {
                if (component->entityId == event.id || component->alwaysFocus)
                {
                    component->focused = true;
                }
                else
                {
                    component->focused = false;
                }
            }
        }

        _unique_id currentFocus = 0;
    };
}