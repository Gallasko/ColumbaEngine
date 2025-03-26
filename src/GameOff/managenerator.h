#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"

#include "gamefacts.h"

namespace pg
{
    // Todo need to implement
    // -> Storage
    // -> One time Generator/Consumers (Standard event that add or remove a certain resource)
    // -> Harvester (Make the generator/converter automatic)
    // -> Make random timed events that increase some values

    // Component for a mana generator (e.g., your altar)
    struct RessourceGenerator
    {
        RessourceGenerator() {}
        RessourceGenerator(const std::string& id, const std::string& ressource, float productionRate = 0.0f, float capacity = 0.0f) : id(id), ressource(ressource), productionRate(productionRate), capacity(capacity) {}
        RessourceGenerator(const RessourceGenerator& other) : id(other.id), ressource(other.ressource), currentMana(other.currentMana), productionRate(other.productionRate), capacity(other.capacity) {}

        RessourceGenerator& operator=(const RessourceGenerator& other)
        {
            id = other.id;
            ressource = other.ressource;
            currentMana = other.currentMana;
            productionRate = other.productionRate;
            capacity = other.capacity;

            return *this;
        }

        std::string id = "generator";  // Name of the generator

        std::string ressource = "mana";// Ressouce created by this generator

        float currentMana = 0.0f;      // Current stored mana
        float productionRate = 1.0f;   // Mana produced per second
        float capacity = 100.0f;       // Maximum mana the generator can store

        // Todo add support for multipliers
    };

    template <>
    void serialize(Archive& archive, const RessourceGenerator& value);

    template <>
    RessourceGenerator deserialize(const UnserializedObject& serializedString);

    struct NewGeneratorEvent
    {
        RessourceGenerator generator;
    };

    struct RessourceGeneratorSystem : public System<Own<RessourceGenerator>, Listener<NewGeneratorEvent>, Listener<TickEvent>, Listener<StandardEvent>, InitSys, SaveSys>
    {
        virtual void init() override
        {
            addListenerToStandardEvent("res_harvest");
            addListenerToStandardEvent("res_gen_upgrade");
        }

        virtual void save(Archive& archive) override
        {
            std::vector<RessourceGenerator> generators;

            for (const auto& gen : view<RessourceGenerator>())
            {
                generators.push_back(*gen);
            }

            serialize(archive, "generators", generators);
        }

        virtual void load(const UnserializedObject& serializedString) override
        {
            std::vector<RessourceGenerator> generators;

            defaultDeserialize(serializedString, "generators", generators);

            for (const auto& gen : generators)
            {
                auto genEnt = ecsRef->createEntity();

                auto generator = ecsRef->attach<RessourceGenerator>(genEnt, gen);
            }
        }

        virtual std::string getSystemName() const override { return "Resource Generator System"; }

        virtual void onEvent(const NewGeneratorEvent& event) override
        {
            const auto& it = generatorMap.find(event.generator.id);

            if (it != generatorMap.end())
            {
                LOG_ERROR("Ressource Generator", "Generator already exists");
                return;
            }

            auto genEnt = ecsRef->createEntity();

            auto generator = ecsRef->attach<RessourceGenerator>(genEnt, event.generator);

            generatorMap[event.generator.id] = genEnt.id;
        }

        virtual void onEvent(const StandardEvent& event) override
        {
            if (event.name == "res_harvest")
            {
                auto id = event.values.at("id").get<std::string>();

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

        void onManaHarvest(const std::string& id)
        {
            if (generatorMap.find(id) == generatorMap.end())
            {
                LOG_ERROR("RessourceGeneratorHarvest", "Generator with id '" << id << "' not found!");
                return;
            }

            auto entId = generatorMap.at(id);

            auto ent = ecsRef->getEntity(entId);

            if (not ent or (not ent->has<RessourceGenerator>()))
            {
                LOG_ERROR("RessourceGeneratorHarvest", "Entity requested doesn't have a RessourceGenerator component!");
                return;
            }

            auto gen = ent->get<RessourceGenerator>();

            ecsRef->sendEvent(IncreaseFact{gen->ressource, gen->currentMana});
            ecsRef->sendEvent(IncreaseFact{"total_" + gen->ressource, gen->currentMana});

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

                    // Todo check perf, we may only need to update all res only once per execute or even once every 150ms
                    ecsRef->sendEvent(AddFact{gen->id + "_current_res", ElementType(gen->currentMana)});
                }

                deltaTime = 0;
            }

        }

        size_t deltaTime = 0;

        std::unordered_map<std::string, _unique_id> generatorMap;
    };

    /**
     * ConverterComponent: Attach this to an entity that acts as a converter
     */
    struct ConverterComponent
    {
        /** Name of the converter */
        std::string id = "converter";

        /** Ressources to be deducted */
        std::vector<std::string> input;
        /** Ressources to be gained */
        std::vector<std::string> output;

        /** How much of input is required per conversion */
        std::vector<float> cost;
        /** How much of output is granted per conversion */
        std::vector<float> yield;

        // Todo add support for multipliers
    };

    // Standard event name to trigger a conversion.
    // When a converter's UI is clicked, you should send:
    //   StandardEvent("converter_triggered", "id", <converter_entity_id as string>)
    static const std::string ConverterTriggeredEventName = "converter_triggered";

    struct ConverterSystem : public System<Own<ConverterComponent>, Listener<StandardEvent>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Converter System"; }

        virtual void init() override
        {
            addListenerToStandardEvent(ConverterTriggeredEventName);
        }

        virtual void onEvent(const StandardEvent& event) override
        {
            // Check that the event is for a converter trigger.
            if (event.name != ConverterTriggeredEventName)
                return;

            // Expect the event to have an "id" parameter containing the converter entity's id.
            auto it = event.values.find("id");
            if (it == event.values.end())
            {
                LOG_ERROR("ConverterSystem", "No id provided in converter_triggered event.");
                return;
            }

            // Convert the id value to _unique_id (assuming your engine uses string conversion)
            _unique_id convId = event.values.at("id").get<size_t>();

            auto ent = ecsRef->getEntity(convId);
            if (not ent or not ent->has<ConverterComponent>())
            {
                LOG_ERROR("ConverterSystem", "Entity " << convId << " does not have a ConverterComponent!");
                return;
            }
            auto conv = ent->get<ConverterComponent>();

            // Get current amount of resourceIn from the WorldFacts system.
            WorldFacts* wf = ecsRef->getSystem<WorldFacts>();

            bool conversionPosible = true;

            if (conv->cost.size() < conv->input.size() or conv->yield.size() < conv->output.size())
            {
                LOG_ERROR("ConverterSystem", "Invalid converter configuration: " << "cost and yield vectors have different sizes.");
                return;
            }

            for (size_t i = 0; i < conv->input.size(); i++)
            {
                float available = 0.0f;

                auto factIt = wf->factMap.find(conv->input[i]);
                if (factIt != wf->factMap.end())
                {
                    available = factIt->second.get<float>();
                }

                if (available < conv->cost[i])
                {
                    LOG_ERROR("ConverterSystem", "Insufficient " << conv->input[i] << ": " << available << " available, " << conv->cost[i] << " required.");
                    conversionPosible = false;
                    break;
                }
            }

            if (conversionPosible)
            {
                // Deduct resourceIn and add resourceOut.
                for (size_t i = 0; i < conv->input.size(); i++)
                    ecsRef->sendEvent(IncreaseFact{conv->input[i], -conv->cost[i]});

                for (size_t i = 0; i < conv->output.size(); i++)
                {
                    ecsRef->sendEvent(IncreaseFact{conv->output[i], conv->yield[i]});
                    ecsRef->sendEvent(IncreaseFact{"total_" + conv->output[i], conv->yield[i]});
                }
            }
        }
    };
}