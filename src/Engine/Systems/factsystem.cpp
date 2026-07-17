#include "factsystem.h"

namespace pg
{
    void WorldFacts::save(Archive& archive)
    {
        serialize(archive, "factMap", factMap);
        LOG_INFO("WorldFacts", "saved " << factMap.size() << " facts");
    }

    void WorldFacts::load(const UnserializedObject& serializedString)
    {
        defaultDeserialize(serializedString, "factMap", factMap);
        LOG_INFO("WorldFacts", "loaded " << factMap.size() << " facts");
    }

    bool FactChecker::check(const std::unordered_map<std::string, ElementType>& map) const
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

    void WorldFacts::onEvent(const AddFact& event)
    {
        factMap[event.name] = event.value;
        factMetadata[event.name] = event.metadata;
        changedFacts.push_back(event.name);
        changed = true;
    }

    void WorldFacts::onEvent(const RemoveFact& event)
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

    void WorldFacts::onEvent(const IncreaseFact& event)
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

    void WorldFacts::setDefaultFact(const std::string& name, const ElementType& value)
    {
        if (factMap.find(name) == factMap.end())
            factMap[name] = value;
    }

    void WorldFacts::execute()
    {
        if (changed)
        {
            ecsRef->sendEvent(WorldFactsUpdate{&factMap, changedFacts});
            changedFacts.clear();
            changed = false;
        }
    }
}
