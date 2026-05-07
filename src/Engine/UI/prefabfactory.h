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
        struct Entry
        {
            std::string name;
            ElementType::UnionType type;
            ElementType defaultValue;
        };

        std::vector<Entry> entries;
    };

    using PrefabFactoryFn = std::function<EntityRef(EntitySystem*, const PrefabParams&)>;

    class PrefabFactoryRegistry : public System<>
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

    inline float getParamFloat(const PrefabParams& p, const std::string& key, float fallback = 0.0f)
    {
        auto it = p.find(key);
        if (it == p.end() || it->second.isEmpty())
            return fallback;

        const auto& v = it->second;
        switch (v.type)
        {
            case ElementType::UnionType::FLOAT:  return v.get<float>();
            case ElementType::UnionType::DOUBLE: return static_cast<float>(v.get<double>());
            case ElementType::UnionType::INT:    return static_cast<float>(v.get<int>());
            case ElementType::UnionType::SIZE_T: return static_cast<float>(v.get<size_t>());
            default: return fallback;
        }
    }

    inline int getParamInt(const PrefabParams& p, const std::string& key, int fallback = 0)
    {
        auto it = p.find(key);
        if (it == p.end() || it->second.isEmpty())
            return fallback;

        const auto& v = it->second;
        switch (v.type)
        {
            case ElementType::UnionType::INT:    return v.get<int>();
            case ElementType::UnionType::SIZE_T: return static_cast<int>(v.get<size_t>());
            case ElementType::UnionType::FLOAT:  return static_cast<int>(v.get<float>());
            case ElementType::UnionType::DOUBLE: return static_cast<int>(v.get<double>());
            default: return fallback;
        }
    }

    inline std::string getParamString(const PrefabParams& p, const std::string& key, const std::string& fallback = "")
    {
        auto it = p.find(key);
        if (it == p.end() || it->second.isEmpty() || it->second.type != ElementType::UnionType::STRING)
            return fallback;
        return it->second.get<std::string>();
    }

    inline bool getParamBool(const PrefabParams& p, const std::string& key, bool fallback = false)
    {
        auto it = p.find(key);
        if (it == p.end() || it->second.isEmpty() || it->second.type != ElementType::UnionType::BOOL)
            return fallback;
        return it->second.get<bool>();
    }
}
