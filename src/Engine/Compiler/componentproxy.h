#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "ECS/entitysystem_fwd.h"

#include "object.h"

namespace pg
{
    /**
     * @brief Kind of value a proxy property holds, used by the editor to pick a widget
     */
    enum class PropertyType : uint8_t
    {
        Float,
        Double,
        Int,
        Bool,
        String,
        UnsignedInt,
        Vector3D,
        Vector4D,
        UniqueId,
        Enum,
        Custom
    };

    /**
     * @brief One script-visible property of a component
     *
     * All accessors are plain function pointers taking the type-erased component.
     * The value setter goes through the component's own setter so that change events still fire.
     */
    struct PropertyMetadata
    {
        using GetterFn = Value (*)(void* component, VM* vm);
        using SetterFn = void (*)(void* component, VM* vm, Value value);

        // String accessors, used by the editor inspector
        using SGetterFn = std::string (*)(void* component);
        using SSetterFn = void (*)(void* component, const std::string& value);

        std::string name;

        PropertyType type = PropertyType::Custom;

        bool writable = false;

        GetterFn getter = nullptr;

        // Always set on a writable property
        SetterFn setter = nullptr;

        SGetterFn sGetter = nullptr;

        SSetterFn sSetter = nullptr;
    };

    /**
     * @brief Everything the VM needs to expose one component type to scripts
     *
     * A proxy instance points at one of these, so the lookup by type name only happens when the proxy is created.
     */
    struct ComponentProxyMetadata
    {
        using RetrieverFn = void* (*)(EntitySystem* ecs, _unique_id entityId);

        using DynamicGetterFn = Value (*)(void* component, const std::string& name, VM* vm);
        using DynamicSetterFn = void (*)(void* component, const std::string& name, VM* vm, Value value);

        void addProperty(const PropertyMetadata& property)
        {
            properties.push_back(property);
        }

        /**
         * @brief Find a property by name
         *
         * This is the only place that maps a property name to its accessors, change the strategy here if it shows up in a profile.
         *
         * @return The property, or nullptr if the component has no such property
         */
        const PropertyMetadata* findProperty(const std::string& name) const
        {
            for (const auto& property : properties)
            {
                if (property.name == name)
                    return &property;
            }

            return nullptr;
        }

        std::string componentTypeName;

        // Kept in declaration order
        std::vector<PropertyMetadata> properties;

        // Fetch the component of an entity, nullptr when the type is only reachable by its runtime name (StandardComponent)
        RetrieverFn retriever = nullptr;

        // Fallbacks for components whose properties are only known at runtime (StandardComponent)
        DynamicGetterFn dynamicGetter = nullptr;

        DynamicSetterFn dynamicSetter = nullptr;
    };

    /**
     * @brief Registry of the component types that scripts can access through a proxy
     *
     * Filled at static initialization time by the generated component files.
     * Registered metadata is never moved nor removed, a pointer to it stays valid for the whole program.
     */
    class ComponentProxyRegistry
    {
    public:
        static ComponentProxyRegistry& instance()
        {
            static ComponentProxyRegistry registry;

            return registry;
        }

        void registerMetadata(const ComponentProxyMetadata& metadata)
        {
            registered[metadata.componentTypeName] = metadata;
        }

        /** @return The metadata of the type, or nullptr if the type has no proxy support */
        const ComponentProxyMetadata* findMetadata(const std::string& typeName) const
        {
            auto it = registered.find(typeName);

            if (it == registered.end())
                return nullptr;

            return &it->second;
        }

        bool hasMetadata(const std::string& typeName) const
        {
            return registered.find(typeName) != registered.end();
        }

    private:
        std::unordered_map<std::string, ComponentProxyMetadata> registered;
    };

    /**
     * @brief Script-side view of a C++ component
     *
     * A proxy is an instance of the ComponentProxy class that holds no field: it carries the component pointer
     * and its metadata (ObjInstance::proxyTarget and proxyMeta), and every property access is forwarded to the component.
     * Nothing is copied, and a write goes through the component setter.
     *
     * A proxy does not own the component and does not track its lifetime.
     */
    class ComponentProxy
    {
    public:
        /**
         * @brief Create the ComponentProxy class in a VM
         *
         * Called once per VM, before any proxy is created.
         */
        static void registerWithVM(VM* vm);

        /**
         * @brief Create a proxy on a component
         *
         * @param vm VM that will own the proxy instance
         * @param metadata Metadata of the component type, from the ComponentProxyRegistry
         * @param component Pointer to the C++ component
         * @return The proxy instance, owned by the caller
         */
        static Value createProxy(VM* vm, const ComponentProxyMetadata* metadata, void* component);

        /**
         * @brief Read a property of the component behind a proxy
         *
         * @return The value, owned by the caller. An unknown property reads as -1
         */
        static Value getProperty(VM* vm, ObjInstance* proxy, const std::string& name);

        /**
         * @brief Write a property of the component behind a proxy
         *
         * An unknown property is ignored.
         *
         * @return false if the property is read-only
         */
        static bool setProperty(VM* vm, ObjInstance* proxy, const std::string& name, Value value);
    };
}
