#pragma once

#include "ECS/entitysystem.h"
#include "ECS/system.h"

#include "Helpers/helpers.h"

#include "Memory/elementtype.h"

namespace pg
{
    struct AddFact
    {
        std::string name;

        ElementType value;
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
        IncreaseFact(const IncreaseFact& other) : name(other.name), value(other.value) {}

        IncreaseFact& operator=(const IncreaseFact& other)
        {
            name = other.name;
            value = other.value;

            return *this;
        }

        std::string name;

        ElementType value{1};
    };

    template <>
    void serialize(Archive& archive, const AddFact& value);

    template <>
    void serialize(Archive& archive, const RemoveFact& value);

    template <>
    void serialize(Archive& archive, const IncreaseFact& value);

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

    const static std::unordered_map<std::string, FactCheckEquality> stringToEquality = invertMap(equalityToString);

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

    struct WorldFacts : public System<Listener<AddFact>, Listener<RemoveFact>, Listener<IncreaseFact>, SaveSys>
    {
        virtual std::string getSystemName() const override { return "WorldFacts"; }

        virtual void save(Archive& archive) override
        {
            serialize(archive, "worldFacts", factMap);
        }

        virtual void load(const UnserializedObject& serializedString) override
        {
            defaultDeserialize(serializedString, "worldFacts", factMap);
        }

        virtual void onEvent(const AddFact& event) override
        {
            factMap[event.name] = event.value;

            changedFacts.push_back(event.name);

            changed = true;
        }

        virtual void onEvent(const RemoveFact& event) override
        {
            auto it = factMap.find(event.name);

            if (it != factMap.end())
            {
                factMap.erase(it);
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
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Game Facts", "Trying to increase a non integer value : " << event.name);
                    return;
                }
            }
            else
            {
                factMap[event.name] = event.value;
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
                ecsRef->sendEvent(WorldFactsUpdate{&factMap, changedFacts});

                changedFacts.clear();
                changed = false;
            }
        }

        std::vector<std::string> changedFacts;

        bool changed = false;

        std::unordered_map<std::string, ElementType> factMap;

        // ===== HELPER METHODS =====

        // READ operations (direct access, fast)
        template<typename T>
        T getFact(const std::string& name, T defaultValue = T{}) const 
        {
            auto it = factMap.find(name);
            return (it != factMap.end()) ? it->second.get<T>() : defaultValue;
        }
        
        bool hasFact(const std::string& name) const 
        {
            return factMap.find(name) != factMap.end();
        }
        
        // WRITE operations (use events, reactive)
        template<typename T>
        void setFact(const std::string& name, T value) 
        {
            ecsRef->sendEvent(AddFact{name, ElementType(value)});
        }
        
        template<typename T>
        void increaseFact(const std::string& name, T amount) 
        {
            ecsRef->sendEvent(IncreaseFact{name, ElementType(amount)});
        }
        
        template<typename T>
        void setFactIfNotExists(const std::string& name, T value) 
        {
            if (!hasFact(name)) {
                setFact(name, value);
            }
        }
        
        // Generic resource operations (no game-specific logic)
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
        
        // Generic statistics operations
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