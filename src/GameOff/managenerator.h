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

    // Event for upgrading the mana generator.
    struct UpgradeManaGeneratorProductionEvent
    {
        UpgradeManaGeneratorProductionEvent(_unique_id id, float upgradeAmount) : id(id), upgradeAmount(upgradeAmount) {}
        UpgradeManaGeneratorProductionEvent(const UpgradeManaGeneratorProductionEvent& other) : id(other.id), upgradeAmount(other.upgradeAmount) {}

        UpgradeManaGeneratorProductionEvent& operator=(const UpgradeManaGeneratorProductionEvent& other)
        {
            id = other.id;
            upgradeAmount = other.upgradeAmount;
            return *this;
        }

        _unique_id id;       // ID of the mana generator to upgrade.
        float upgradeAmount; // Amount by which to boost production (and possibly capacity).
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

    struct ManaGeneratorSystem : public System<Own<ManaGenerator>, Listener<TickEvent>, Listener<OnManaGeneratorHarvest>, Listener<UpgradeManaGeneratorProductionEvent>>
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

        virtual void onEvent(const UpgradeManaGeneratorProductionEvent& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent or (not ent->has<ManaGenerator>()))
            {
                LOG_ERROR("UpgradeManaGenerator", "Entity requested doesn't have a ManaGenerator component!");
                return;
            }

            auto gen = ent->get<ManaGenerator>();

            // Upgrade: Increase production rate and capacity.
            gen->productionRate += event.upgradeAmount;

            LOG_INFO("UpgradeManaGenerator", "Generator upgraded: new productionRate = " << gen->productionRate);
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