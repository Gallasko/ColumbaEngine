#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ECS/entitysystem.h"
#include "ECS/system.h"

#include "Helpers/helpers.h"

#include "Memory/elementtype.h"

namespace pg
{
    struct FactMetadata
    {
        std::unordered_map<std::string, ElementType> meta;
    };

    struct AddFact
    {
        std::string name;

        ElementType value;

        FactMetadata metadata = {};
    };

    struct RemoveFact
    {
        std::string name;
    };

    struct IncreaseFact
    {
        IncreaseFact(const std::string& name = "Noop") : name(name), value(1) {}
        template <typename Type>
        IncreaseFact(const std::string& name, Type value = 1) : name(name), value(value) {}
        IncreaseFact(const IncreaseFact& other) : name(other.name), value(other.value), metadata(other.metadata) {}

        IncreaseFact& operator=(const IncreaseFact& other)
        {
            name = other.name;
            value = other.value;
            metadata = other.metadata;

            return *this;
        }

        std::string name;

        ElementType value{1};

        FactMetadata metadata = {};
    };

    template <>
    void serialize(Archive& archive, const FactMetadata& value);

    template <>
    void serialize(Archive& archive, const AddFact& value);

    template <>
    void serialize(Archive& archive, const RemoveFact& value);

    template <>
    void serialize(Archive& archive, const IncreaseFact& value);

    template <>
    FactMetadata deserialize(const UnserializedObject& serializedString);

    template <>
    AddFact deserialize(const UnserializedObject& serializedString);

    template <>
    RemoveFact deserialize(const UnserializedObject& serializedString);

    template <>
    IncreaseFact deserialize(const UnserializedObject& serializedString);

    struct WorldFactsUpdate
    {
        std::unordered_map<std::string, ElementType> *factMap;

        std::vector<std::string> changedFacts;
    };

    enum class FactCheckEquality
    {
        None = 0,
        Lesser,
        LesserEqual,
        Equal,
        NotEqual,
        Greater,
        GreaterEqual,
    };

    const static std::unordered_map<FactCheckEquality, std::string> equalityToString = {
        {FactCheckEquality::None, "None"},
        {FactCheckEquality::Lesser, "Lesser"},
        {FactCheckEquality::LesserEqual, "LesserEqual"},
        {FactCheckEquality::Equal, "Equal"},
        {FactCheckEquality::NotEqual, "NotEqual"},
        {FactCheckEquality::Greater, "Greater"},
        {FactCheckEquality::GreaterEqual, "GreaterEqual"},
    };

    // Built once in gamefacts.cpp
    extern const std::unordered_map<std::string, FactCheckEquality> stringToEquality;

    struct FactChecker
    {
        FactChecker() {}
        template <typename Type>
        FactChecker(const std::string& name, const Type& value, const FactCheckEquality& equality) : name(name), value(value), equality(equality) {}
        FactChecker(const FactChecker& other) : name(other.name), value(other.value), equality(other.equality) {}

        FactChecker& operator=(const FactChecker& other)
        {
            name = other.name;
            value = other.value;
            equality = other.equality;

            return *this;
        }

        std::string name;

        ElementType value;

        FactCheckEquality equality = FactCheckEquality::None;

        bool check(const std::unordered_map<std::string, ElementType>& map) const
        {
            const auto& it = map.find(name);

            if (it != map.end())
            {
                try
                {
                    switch (equality)
                    {
                    case FactCheckEquality::Lesser:
                        return (it->second < value).isTrue();
                        break;

                    case FactCheckEquality::LesserEqual:
                        return (it->second <= value).isTrue();
                        break;

                    case FactCheckEquality::Equal:
                        return (it->second == value).isTrue();
                        break;

                    case FactCheckEquality::NotEqual:
                        return (it->second != value).isTrue();
                        break;

                    case FactCheckEquality::Greater:
                        return (it->second > value).isTrue();
                        break;

                    case FactCheckEquality::GreaterEqual:
                        return (it->second >= value).isTrue();
                        break;

                    case FactCheckEquality::None:
                    default:
                        return false;
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Fact check", "Error happend during fact check: " << e.what());
                    return false;
                }
            }

            return false;
        }
    };

    template <>
    void serialize(Archive& archive, const FactChecker& value);

    template <>
    FactChecker deserialize(const UnserializedObject& serializedString);

    struct WorldFacts : public System<Listener<AddFact>, Listener<RemoveFact>, Listener<IncreaseFact>, SaveSys>
    {
        virtual std::string getSystemName() const override { return "WorldFacts"; }

        virtual void save(Archive& archive) override
        {
            serialize(archive, "worldFacts", factMap);
            serialize(archive, "factMetadata", factMetadata);
        }

        virtual void load(const UnserializedObject& serializedString) override
        {
            // Saves written through the former Systems/factsystem.h used this key
            defaultDeserialize(serializedString, "factMap", factMap);

            defaultDeserialize(serializedString, "worldFacts", factMap);
            defaultDeserialize(serializedString, "factMetadata", factMetadata);
        }

        virtual void onEvent(const AddFact& event) override
        {
            factMap[event.name] = event.value;
            factMetadata[event.name] = event.metadata;

            changedFacts.push_back(event.name);

            changed = true;
        }

        virtual void onEvent(const RemoveFact& event) override
        {
            auto it = factMap.find(event.name);

            if (it != factMap.end())
            {
                factMap.erase(it);
                factMetadata.erase(event.name);
            }

            changedFacts.push_back(event.name);

            changed = true;
        }

        virtual void onEvent(const IncreaseFact& event) override
        {
            const auto& it = factMap.find(event.name);

            if (it != factMap.end())
            {
                try
                {
                    factMap[event.name] = it->second + event.value;
                    // Keep existing metadata, don't update it
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Game Facts", "Trying to increase a non integer value : " << event.name);
                    return;
                }
            }
            else
            {
                // Fact doesn't exist - create with provided metadata
                factMap[event.name] = event.value;
                factMetadata[event.name] = event.metadata;
            }

            changedFacts.push_back(event.name);

            changed = true;
        }

        void setDefaultFact(const std::string& name, const ElementType& value)
        {
            auto it = factMap.find(name);

            if (it == factMap.end())
            {
                factMap[name] = value;
            }
        }

        template <typename Type>
        void setDefaultFact(const std::string& name, Type value)
        {
            setDefaultFact(name, ElementType{value});
        }

        virtual void execute() override
        {
            if (changed)
            {
                // Taken before the event goes out: delivered on the spot (the settle phase), its listeners
                // may set facts of their own, which are for the next update
                std::vector<std::string> names;
                names.swap(changedFacts);

                changed = false;

                ecsRef->sendEvent(WorldFactsUpdate{&factMap, names});
            }
        }

        std::vector<std::string> changedFacts;

        bool changed = false;

        std::unordered_map<std::string, ElementType> factMap;

        std::unordered_map<std::string, FactMetadata> factMetadata;

        // Reads are direct
        template <typename Type>
        Type getFact(const std::string& name, Type defaultValue = Type{}) const
        {
            auto it = factMap.find(name);

            return (it != factMap.end()) ? it->second.get<Type>() : defaultValue;
        }

        bool hasFact(const std::string& name) const
        {
            return factMap.find(name) != factMap.end();
        }

        // Writes go through events
        template <typename Type>
        void setFact(const std::string& name, Type value)
        {
            ecsRef->sendEvent(AddFact{name, ElementType(value)});
        }

        template <typename Type>
        void increaseFact(const std::string& name, Type amount)
        {
            ecsRef->sendEvent(IncreaseFact{name, ElementType(amount)});
        }

        template <typename Type>
        void setFactIfNotExists(const std::string& name, Type value)
        {
            if (not hasFact(name))
                setFact(name, value);
        }

        float getResource(const std::string& resource) const
        {
            return getFact<float>(resource, 0.0f);
        }

        bool canAfford(const std::string& resource, float cost) const
        {
            return getResource(resource) >= cost;
        }

        void spendResource(const std::string& resource, float cost)
        {
            increaseFact(resource, -cost);
        }

        void addResource(const std::string& resource, float amount)
        {
            increaseFact(resource, amount);
        }

        void incrementStat(const std::string& statName, float amount = 1.0f)
        {
            increaseFact(statName, amount);
        }

        float getStat(const std::string& statName) const
        {
            return getFact<float>(statName, 0.0f);
        }
    };
}
