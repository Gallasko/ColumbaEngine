#pragma once

#include "ECS/system.h"
#include "ECS/entitysystem.h"
#include "Memory/elementtype.h"
#include "UI/prefabspec.h"

#include <any>
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
            // Basic constructors — entries with a default value are Optional by default
            // (the default value is the fallback when the caller omits the param). Pass
            // `Requirement::Required` explicitly to force the caller to provide the param
            // anyway (the default then acts only as a type/value placeholder).
            Entry(const std::string& name, int defaultValue, Requirement requirement = Requirement::Optional)
                : name(name), type(UnionType::INT), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, float defaultValue, Requirement requirement = Requirement::Optional)
                : name(name), type(UnionType::FLOAT), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, double defaultValue, Requirement requirement = Requirement::Optional)
                : name(name), type(UnionType::DOUBLE), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, size_t defaultValue, Requirement requirement = Requirement::Optional)
                : name(name), type(UnionType::SIZE_T), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, bool defaultValue, Requirement requirement = Requirement::Optional)
                : name(name), type(UnionType::BOOL), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, const std::string& defaultValue, Requirement requirement = Requirement::Optional)
                : name(name), type(UnionType::STRING), defaultValue(defaultValue), requirement(requirement) {}

            Entry(const std::string& name, const char* defaultValue, Requirement requirement = Requirement::Optional)
                : name(name), type(UnionType::STRING), defaultValue(std::string(defaultValue)), requirement(requirement) {}

            // Constructors for values with NO default — these are Required by default; the
            // caller MUST provide the param in their PrefabParams list or the build fails.
            Entry(const std::string& name, UnionType type, Requirement requirement = Requirement::Required)
                : name(name), type(type), requirement(requirement) {}

            std::string name;
            UnionType type;
            ElementType defaultValue;
            Requirement requirement = Requirement::Required;
        };

        std::vector<Entry> entries;
    };

    /**
     * Per-build state the builder threads through the factories.
     *
     *  - `handles`: where a factory's `FactoryResult::handle` lands, keyed by the node's name.
     *    Owned by the `PrefabBuildResult` of the running `buildTree()`; null when a factory is
     *    invoked directly through `PrefabFactoryRegistry::build()`.
     *  - `inherited`: the props the parent handed down (`FactoryResult::childDefaults` of the
     *    enclosing node). They are already merged into the NodeSpec the factory receives; this
     *    only tells a factory which values were inherited rather than written by the caller.
     */
    struct BuildContext
    {
        std::unordered_map<std::string, std::any>* handles = nullptr;
        ElementMap inherited;
    };

    /**
     * What a factory gives back to the builder.
     *
     *  - `entity`:        the realised leaf; becomes the wrap's mainEntity. Empty on failure.
     *  - `slot`:          optional. A layout entity (VerticalLayout / HorizontalLayout) that
     *                     receives the node's children through `addEntity` instead of the
     *                     sibling-anchoring path. The Chronicle panel's body is one.
     *  - `childDefaults`: optional. Props merged UNDER every child's own props (an explicit
     *                     child value wins), e.g. `{width: innerWidth, z: contentZ}`.
     *  - `handle`:        optional. The builder's rich result struct (a `Panel`, a `Label`, ...),
     *                     exposed by name in `PrefabBuildResult::handles` so callers keep every
     *                     runtime setter the struct offers.
     */
    struct FactoryResult
    {
        EntityRef   entity;
        EntityRef   slot;
        ElementMap  childDefaults;
        std::any    handle;
    };

    // The simple contract: props in, one entity out. Kept for the engine primitives and any
    // existing factory; wrapped into the extended contract on registration.
    using PrefabFactoryFn = std::function<EntityRef(EntitySystem*, const PrefabParams&)>;

    // The extended contract: the whole NodeSpec (props already merged with the schema defaults
    // and the parent's childDefaults, plus `records`; `children` are NOT passed — the builder
    // owns recursion) and the build context in; a FactoryResult out.
    using PrefabFactoryFnEx = std::function<FactoryResult(EntitySystem*, const NodeSpec&, BuildContext&)>;

    class PrefabFactoryRegistry : public System<StoragePolicy>
    {
    public:
        virtual std::string getSystemName() const override { return "Prefab Factory Registry"; }

        void registerFactory(const std::string& name, ParamSchema schema, PrefabFactoryFn fn)
        {
            PrefabFactoryFnEx wrapped = [fn = std::move(fn)](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
            {
                FactoryResult result;
                result.entity = fn(ecs, spec.props);
                return result;
            };

            registerFactory(name, std::move(schema), std::move(wrapped));
        }

        void registerFactory(const std::string& name, ParamSchema schema, PrefabFactoryFnEx fn)
        {
            schemas[name] = std::move(schema);
            factories[name] = std::move(fn);
        }

        bool hasFactory(const std::string& name) const
        {
            return factories.find(name) != factories.end();
        }

        // Full build: required-param check, schema defaults merged under the spec's props, then
        // the factory. `spec.children` are ignored here (the builder recurses, not the factory).
        FactoryResult buildEx(const NodeSpec& spec, BuildContext& ctx)
        {
            auto it = factories.find(spec.kind);
            if (it == factories.end())
            {
                LOG_ERROR("Prefab Factory Registry", "No factory registered with name: " << spec.kind);
                return FactoryResult{};
            }

            // Enforce required params: every entry marked `Required` in the schema must be
            // present in the caller's `props` map. Schema defaults do NOT satisfy a Required
            // entry — the contract is that the caller explicitly opts in.

            bool allRequiredParamsPresent = true;

            auto schemaIt = schemas.find(spec.kind);
            if (schemaIt != schemas.end())
            {
                for (const auto& entry : schemaIt->second.entries)
                {
                    if (entry.requirement == ParamSchema::Requirement::Required and
                        spec.props.find(entry.name) == spec.props.end())
                    {
                        LOG_ERROR("Prefab Factory Registry",
                            "Factory '" << spec.kind << "' missing required parameter: '" << entry.name << "'");

                        allRequiredParamsPresent = false;
                    }
                }
            }

            if (not allRequiredParamsPresent)
                return FactoryResult{};

            NodeSpec merged;
            merged.kind    = spec.kind;
            merged.name    = spec.name;
            merged.props   = mergeWithDefaults(spec.kind, spec.props);
            merged.records = spec.records;

            return it->second(this->ecsRef, merged, ctx);
        }

        EntityRef build(const std::string& name, const PrefabParams& params)
        {
            NodeSpec spec;
            spec.kind  = name;
            spec.props = params;

            BuildContext ctx;
            return buildEx(spec, ctx).entity;
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

        std::unordered_map<std::string, PrefabFactoryFnEx> factories;
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
