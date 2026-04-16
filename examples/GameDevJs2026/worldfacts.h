#pragma once

// Lightweight port of GameOff's gamefacts.h for GameDevJs2026.
// Saving/loading is intentionally dropped here; only the runtime fact
// tracking + FactChecker is retained for recipe-unlock gating.

#include "ECS/entitysystem.h"
#include "ECS/system.h"

#include "Helpers/helpers.h"

#include "Memory/elementtype.h"

#include <string>
#include <unordered_map>
#include <vector>

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
        IncreaseFact(const std::string& name, Type value) : name(name), value(value) {}
        IncreaseFact(const IncreaseFact& other)
            : name(other.name), value(other.value), metadata(other.metadata) {}

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

    struct WorldFactsUpdate
    {
        std::unordered_map<std::string, ElementType>* factMap;
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

    struct FactChecker
    {
        FactChecker() {}
        template <typename Type>
        FactChecker(const std::string& n, const Type& v, const FactCheckEquality& eq)
            : name(n), value(v), equality(eq) {}
        FactChecker(const FactChecker& other)
            : name(other.name), value(other.value), equality(other.equality) {}

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
            if (it == map.end())
                return false;

            try
            {
                switch (equality)
                {
                case FactCheckEquality::Lesser:       return (it->second <  value).isTrue();
                case FactCheckEquality::LesserEqual:  return (it->second <= value).isTrue();
                case FactCheckEquality::Equal:        return (it->second == value).isTrue();
                case FactCheckEquality::NotEqual:     return (it->second != value).isTrue();
                case FactCheckEquality::Greater:      return (it->second >  value).isTrue();
                case FactCheckEquality::GreaterEqual: return (it->second >= value).isTrue();
                case FactCheckEquality::None:
                default:
                    return false;
                }
            }
            catch (const std::exception&)
            {
                return false;
            }
        }
    };

    struct WorldFacts : public System<Listener<AddFact>,
                                      Listener<RemoveFact>,
                                      Listener<IncreaseFact>>
    {
        virtual std::string getSystemName() const override { return "WorldFacts"; }

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
            auto it = factMap.find(event.name);
            if (it != factMap.end())
            {
                try
                {
                    factMap[event.name] = it->second + event.value;
                }
                catch (const std::exception&)
                {
                    return;
                }
            }
            else
            {
                factMap[event.name] = event.value;
                factMetadata[event.name] = event.metadata;
            }
            changedFacts.push_back(event.name);
            changed = true;
        }

        void setDefaultFact(const std::string& name, const ElementType& value)
        {
            if (factMap.find(name) == factMap.end())
                factMap[name] = value;
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
        std::unordered_map<std::string, FactMetadata> factMetadata;

        // ===== Helpers =====

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
            if (not hasFact(name))
                setFact(name, value);
        }
    };
}
