#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"

#include "gamefacts.h"

namespace pg
{
    // Component for a mana generator (e.g., your altar)
    struct ManaGenerator
    {
        float currentMana = 0.0f;      // Current stored mana
        float productionRate = 1.0f;   // Mana produced per second
        float capacity = 100.0f;       // Maximum mana the generator can store
    };

    struct OnManaGeneratorHarvest
    {
        OnManaGeneratorHarvest(_unique_id id) : id(id) {}
        OnManaGeneratorHarvest(const OnManaGeneratorHarvest& other) : id(other.id) {}

        OnManaGeneratorHarvest& operator=(const OnManaGeneratorHarvest& other)
        {
            id = other.id;

            return *this;
        }

        // Unique ID of the mana generator that was harvested
        _unique_id id;
    };

    struct ManaGeneratorSystem : public System<Own<ManaGenerator>, Listener<TickEvent>, Listener<OnManaGeneratorHarvest>>
    {
        virtual std::string getSystemName() const override { return "Mana Generator System"; }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void onEvent(const OnManaGeneratorHarvest& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent or (not ent->has<ManaGenerator>()))
            {
                LOG_ERROR("ManaGeneratorHarvest", "Entity requested doesn't have a ManaGenerator component!");
                return;
            }

            auto gen = ent->get<ManaGenerator>();
            
            ecsRef->sendEvent(IncreaseFact{"mana", gen->currentMana});

            gen->currentMana = 0.0f;
        }

        virtual void execute() override
        {
            if (deltaTime > 0)
            {
                auto df = deltaTime / 1000.0f;

                // For each mana generator in the scene, increase the current mana.
                for (auto gen : view<ManaGenerator>())
                {
                    gen->currentMana += gen->productionRate * df;

                    if (gen->currentMana > gen->capacity)
                        gen->currentMana = gen->capacity;
                }

                deltaTime = 0;
            }
            
        }

        size_t deltaTime = 0;
    };
}