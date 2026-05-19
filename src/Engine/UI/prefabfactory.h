#pragma once

#include "ECS/system.h"
#include "ECS/entitysystem.h"
#include "Memory/elementtype.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pg
{
    using PrefabParams = ElementMap;

    struct ParamSchema
    {
        enum class Requirement
        {
            Optional,
            Required,
        };

        struct Entry
        {
            // Basic constructors
            Entry(const std::string& name, int defaultValue, Requirement requirement = Requirement::Required)
                : name(name), type(UnionType::INT), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, float defaultValue, Requirement requirement = Requirement::Required)
                : name(name), type(UnionType::FLOAT), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, double defaultValue, Requirement requirement = Requirement::Required)
                : name(name), type(UnionType::DOUBLE), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, size_t defaultValue, Requirement requirement = Requirement::Required)
                : name(name), type(UnionType::SIZE_T), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, bool defaultValue, Requirement requirement = Requirement::Required)
                : name(name), type(UnionType::BOOL), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, const std::string& defaultValue, Requirement requirement = Requirement::Required)
                : name(name), type(UnionType::STRING), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, const char* defaultValue, Requirement requirement = Requirement::Required)
                : name(name), type(UnionType::STRING), defaultValue(std::string(defaultValue)), requirement(requirement) {}

            // Constructors for values with no default
            Entry(const std::string& name, UnionType type, Requirement requirement = Requirement::Required)
                : name(name), type(type), requirement(requirement) {}

            std::string name;
            UnionType type;
            ElementType defaultValue;
            Requirement requirement = Requirement::Required;
        };

        std::vector<Entry> entries;
    };

    using PrefabFactoryFn = std::function<EntityRef(EntitySystem*, const PrefabParams&)>;

    class PrefabFactoryRegistry : public System<StoragePolicy>
    {
    public:
        virtual std::string getSystemName() const override { return "Prefab Factory Registry"; }

        void registerFactory(const std::string& name, ParamSchema schema, PrefabFactoryFn fn)
        {
            schemas[name] = std::move(schema);
            factories[name] = std::move(fn);
        }

        bool hasFactory(const std::string& name) const
        {
            return factories.find(name) != factories.end();
        }

        EntityRef build(const std::string& name, const PrefabParams& params)
        {
            auto it = factories.find(name);
            if (it == factories.end())
            {
                LOG_ERROR("Prefab Factory Registry", "No factory registered with name: " << name);
                return EntityRef{};
            }

            return it->second(this->ecsRef, mergeWithDefaults(name, params));
        }

        EntityRef build(const std::string& name)
        {
            return build(name, PrefabParams{});
        }

        std::vector<std::string> listFactories() const
        {
            std::vector<std::string> names;
            names.reserve(factories.size());

            for (const auto& kv : factories)
                names.push_back(kv.first);

            return names;
        }

        const ParamSchema& schemaOf(const std::string& name) const
        {
            static const ParamSchema empty;
            auto it = schemas.find(name);
            return it != schemas.end() ? it->second : empty;
        }

    private:
        PrefabParams mergeWithDefaults(const std::string& name, const PrefabParams& params) const
        {
            PrefabParams merged;
            auto schemaIt = schemas.find(name);

            if (schemaIt != schemas.end())
            {
                for (const auto& entry : schemaIt->second.entries)
                    merged[entry.name] = entry.defaultValue;
            }

            for (const auto& kv : params)
                merged[kv.first] = kv.second;

            return merged;
        }

        std::unordered_map<std::string, PrefabFactoryFn> factories;
        std::unordered_map<std::string, ParamSchema> schemas;
    };

    inline bool hasParam(const PrefabParams& p, const std::string& key)
    {
        return p.find(key) != p.end();
    }

    inline ElementType getParam(const PrefabParams& p, const std::string& key, ElementType fallback = 0.0f)
    {
        auto it = p.find(key);
        if (it == p.end() or it->second.isEmpty())
            return fallback;

        return it->second;
    }

    inline float getParamFloat(const PrefabParams& p, const std::string& key, float fallback = 0.0f)
    {
        auto it = p.find(key);
        if (it == p.end() or it->second.isEmpty())
            return fallback;

        const auto& v = it->second;

        switch (v.type)
        {
            case UnionType::FLOAT:
                return v.get<float>();
            case UnionType::DOUBLE:
                return static_cast<float>(v.get<double>());
            case UnionType::INT:
                return static_cast<float>(v.get<int>());
            case UnionType::SIZE_T:
                return static_cast<float>(v.get<size_t>());
            default:
                return fallback;
        }
    }

    inline int getParamInt(const PrefabParams& p, const std::string& key, int fallback = 0)
    {
        auto it = p.find(key);
        if (it == p.end() or it->second.isEmpty())
            return fallback;

        const auto& v = it->second;
        switch (v.type)
        {
            case UnionType::INT:
                return v.get<int>();
            case UnionType::SIZE_T:
                return static_cast<int>(v.get<size_t>());
            case UnionType::FLOAT:
                return static_cast<int>(v.get<float>());
            case UnionType::DOUBLE:
                return static_cast<int>(v.get<double>());
            default:
                return fallback;
        }
    }

    inline std::string getParamString(const PrefabParams& p, const std::string& key, const std::string& fallback = "")
    {
        auto it = p.find(key);
        if (it == p.end() or it->second.isEmpty() or it->second.type != UnionType::STRING)
            return fallback;

        return it->second.get<std::string>();
    }

    inline bool getParamBool(const PrefabParams& p, const std::string& key, bool fallback = false)
    {
        auto it = p.find(key);
        if (it == p.end() or it->second.isEmpty() or it->second.type != UnionType::BOOL)
            return fallback;

        return it->second.get<bool>();
    }
}
