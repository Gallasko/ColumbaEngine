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
        RessourceGenerator(const RessourceGenerator& other) : id(other.id), ressource(other.ressource), currentMana(other.currentMana), productionRate(other.productionRate), capacity(other.capacity), active(other.active) {}

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

        bool active = false;

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
            addListenerToStandardEvent("activate_gen");
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

                ecsRef->attach<RessourceGenerator>(genEnt, gen);

                generatorMap[gen.id] = genEnt.id;
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

            ecsRef->attach<RessourceGenerator>(genEnt, event.generator);

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
                auto id = event.values.at("id").get<std::string>();
                auto upgradeAmount = event.values.at("upgradeAmount").get<float>();

                onRessourceGeneratorUpgrade(id, upgradeAmount);
            }
            else if (event.name == "activate_gen")
            {
                auto id = event.values.at("id").get<std::string>();
                auto active = event.values.at("active").get<bool>();

                onActivateGenerator(id, active);
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

            WorldFacts* wf = ecsRef->getSystem<WorldFacts>();
            if (not wf) return;

            float value = 0.0f;
            float maxValue = 0.0f;
            bool hasMax = false;

            auto it = wf->factMap.find(gen->ressource );
            if (it != wf->factMap.end())
            {
                value = it->second.get<float>();
            }

            auto itMax = wf->factMap.find(gen->ressource + "_max_value");
            if (itMax != wf->factMap.end())
            {
                maxValue = itMax->second.get<float>();
                hasMax = true;
            }

            if (hasMax)
            {
                auto availableSpace = maxValue - value;

                if (availableSpace <= 0)
                {
                    LOG_INFO("RessourceGeneratorHarvest", "No space left for ressource '" << gen->ressource << "'");
                    return;
                }

                availableSpace = std::min(availableSpace, gen->currentMana);

                ecsRef->sendEvent(IncreaseFact{gen->ressource, availableSpace});
                ecsRef->sendEvent(IncreaseFact{"total_" + gen->ressource, availableSpace});

                gen->currentMana -= availableSpace;
            }
            else
            {
                ecsRef->sendEvent(IncreaseFact{gen->ressource, gen->currentMana});
                ecsRef->sendEvent(IncreaseFact{"total_" + gen->ressource, gen->currentMana});

                gen->currentMana = 0.0f;
            }
        }

        void onRessourceGeneratorUpgrade(const std::string& id, float amount)
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
                LOG_ERROR("UpgradeRessourceGenerator", "Entity requested doesn't have a RessourceGenerator component!");
                return;
            }

            auto gen = ent->get<RessourceGenerator>();

            // Upgrade: Increase production rate and capacity.
            gen->productionRate += amount;

            LOG_INFO("UpgradeRessourceGenerator", "Generator upgraded: new productionRate = " << gen->productionRate);
        }

        void onActivateGenerator(const std::string& id, bool active)
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
                LOG_ERROR("UpgradeRessourceGenerator", "Entity requested doesn't have a RessourceGenerator component!");
                return;
            }

            auto gen = ent->get<RessourceGenerator>();

            gen->active = active;
        }

        virtual void execute() override
        {
            if (deltaTime > 0)
            {
                auto df = deltaTime / 1000.0f;

                // For each mana generator in the scene, increase the current mana.
                for (auto gen : view<RessourceGenerator>())
                {
                    if (not gen->active)
                        continue;

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
        ConverterComponent() {}
        ConverterComponent(const std::string& id, const std::vector<std::string>& input, const std::vector<std::string>& output, const std::vector<float>& cost, const std::vector<float>& yield, bool active = false)
            : id(id), input(input), output(output), cost(cost), yield(yield), active(active) {}

        ConverterComponent(const ConverterComponent& other) : id(other.id), input(other.input), output(other.output), cost(other.cost), yield(other.yield), active(other.active) {}

        /**
         * @brief Assignment operator for the ConverterComponent class.
         *
         * @param other The ConverterComponent instance to copy from.
         * @return A reference to the current instance after assignment.
         */
        ConverterComponent& operator=(const ConverterComponent& other)
        {
            id = other.id;
            input = other.input;
            output = other.output;
            cost = other.cost;
            yield = other.yield;
            active = other.active;

            return *this;
        }

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

        bool active = false;

        // Todo add support for multipliers
    };

    struct NewConverterEvent
    {
        ConverterComponent converter;
    };

    template <>
    void serialize(Archive& archive, const ConverterComponent& value);

    template <>
    ConverterComponent deserialize(const UnserializedObject& serializedString);

    // Standard event name to trigger a conversion.
    // When a converter's UI is clicked, you should send:
    //   StandardEvent("converter_triggered", "id", <converter_entity_id as string>)
    static const std::string ConverterTriggeredEventName = "converter_triggered";

    struct ConverterSystem : public System<Own<ConverterComponent>, Listener<NewConverterEvent>, Listener<StandardEvent>, InitSys, SaveSys>
    {
        virtual std::string getSystemName() const override { return "Converter System"; }

        virtual void init() override
        {
            addListenerToStandardEvent(ConverterTriggeredEventName);
        }

        virtual void save(Archive& archive) override
        {
            std::vector<ConverterComponent> converters;

            // Iterate over all entities with a ConverterComponent and save their data.
            for (const auto& conv : view<ConverterComponent>())
            {
                converters.push_back(*conv);
            }

            serialize(archive, "converters", converters);
        }

        virtual void load(const UnserializedObject& serializedString) override
        {
            std::vector<ConverterComponent> converters;
            defaultDeserialize(serializedString, "converters", converters);

            // Recreate converter entities based on the saved data.
            for (const auto& conv : converters)
            {
                auto convEnt = ecsRef->createEntity();
                ecsRef->attach<ConverterComponent>(convEnt, conv);
                // Store the entity ID in the converter map for later lookup.
                converterMap[conv.id] = convEnt.id;
            }
        }

        virtual void onEvent(const NewConverterEvent& event) override
        {
            // Check if a converter with the same id already exists.
            if (converterMap.find(event.converter.id) != converterMap.end())
            {
                LOG_ERROR("ConverterSystem", "Converter with id '" << event.converter.id << "' already exists.");
                return;
            }

            // Create a new entity for the converter.
            auto convEnt = ecsRef->createEntity();
            ecsRef->attach<ConverterComponent>(convEnt, event.converter);

            // Save the mapping for later lookup.
            converterMap[event.converter.id] = convEnt.id;
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

        std::unordered_map<std::string, _unique_id> converterMap;
    };
}