#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"

#include "gamefacts.h"

namespace pg
{
    // Component for a mana generator (e.g., your altar)
    struct RessourceGenerator
    {
        std::string ressource = "mana";// Ressouce created by this generator

        float currentMana = 0.0f;      // Current stored mana
        float productionRate = 1.0f;   // Mana produced per second
        float capacity = 100.0f;       // Maximum mana the generator can store
    };

    struct RessourceGeneratorSystem : public System<Own<RessourceGenerator>, Listener<TickEvent>, Listener<StandardEvent>, InitSys>
    {
        virtual void init() override
        {
            addListenerToStandardEvent("res_harvest");
            addListenerToStandardEvent("res_gen_upgrade");
        }

        virtual std::string getSystemName() const override { return "Mana Generator System"; }

        virtual void onEvent(const StandardEvent& event) override
        {
            if (event.name == "res_harvest")
            {
                auto id = event.values.at("id").get<size_t>();

                onManaHarvest(id);
            }
            else if (event.name == "res_gen_upgrade")
            {
                auto id = event.values.at("id").get<size_t>();
                auto upgradeAmount = event.values.at("upgradeAmount").get<float>();

                onRessourceGeneratorUpgrade(id, upgradeAmount);
            }
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        void onManaHarvest(_unique_id id)
        {
            auto ent = ecsRef->getEntity(id);

            if (not ent or (not ent->has<RessourceGenerator>()))
            {
                LOG_ERROR("RessourceGeneratorHarvest", "Entity requested doesn't have a RessourceGenerator component!");
                return;
            }

            auto gen = ent->get<RessourceGenerator>();

            ecsRef->sendEvent(IncreaseFact{gen->ressource, gen->currentMana});

            gen->currentMana = 0.0f;
        }

        void onRessourceGeneratorUpgrade(_unique_id id, float amount)
        {
            auto ent = ecsRef->getEntity(id);

            if (not ent or (not ent->has<RessourceGenerator>()))
            {
                LOG_ERROR("UpgradeRessourceGenerator", "Entity requested doesn't have a RessourceGenerator component!");
                return;
            }

            auto gen = ent->get<RessourceGenerator>();

            // Upgrade: Increase production rate and capacity.
            gen->productionRate += amount;

            LOG_INFO("UpgradeRessourceGenerator", "Generator upgraded: new productionRate = " << gen->productionRate);
        }

        virtual void execute() override
        {
            if (deltaTime > 0)
            {
                auto df = deltaTime / 1000.0f;

                // For each mana generator in the scene, increase the current mana.
                for (auto gen : view<RessourceGenerator>())
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