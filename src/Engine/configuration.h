
#pragma once

#include <typeinfo>
#include <string>
#include <memory>
#include <unordered_map>

#include "serialization.h"
#include "Memory/elementtype.h"

namespace pg
{
    class Configuration
    {
    public:
        typedef std::unique_ptr<Configuration> ConfigurationPtr;

    public:
        //Configuration(const Configuration&& other) : elementMap(std::move(other.elementMap)) {}
        Configuration(const Configuration& other) : elementMap(other.elementMap) {}

        inline static const ConfigurationPtr& config() { static ConfigurationPtr config = ConfigurationPtr(new Configuration(true)); return config; }

        template <typename Type>
        Type get(const std::string& name, const Type& defaultValue) const;

        template <typename Type>
        void set(const std::string& name, const Type& value);

        template <typename Type>
        void set(const char* name, const Type& value);

    private:
        /**
         * @brief Construct a new Configuration object.
         *
         * @param fromSerialization boolean to know if the configuration should be built from serialization or not.
         */
        Configuration(bool fromSerialization = false)
        {
            if (fromSerialization)
                *this = Serializer::getSerializer()->deserializeObject<Configuration>("Config");
        }

        friend void serialize<>(Archive& archive, const Configuration& config);
        friend Configuration deserialize<>(const UnserializedObject& serializedString);

        Configuration& operator=(const Configuration& config)
        {
            for(const auto& element : config.elementMap)
                this->elementMap[element.first] = element.second;

            return *this;
        }

        std::unordered_map<std::string, ElementType> elementMap;
    };

    template <typename Type>
    Type Configuration::get(const std::string& name, const Type& defaultValue) const
    {
        const auto& it = elementMap.find(name);

        if (it == elementMap.end())
            return defaultValue;

        return it->second.get<Type>();
    }

    template <typename Type>
    void Configuration::set(const std::string& name, const Type& value)
    {
        elementMap[name] = ElementType(value);

        // Save the current configuration file inside the serializer
        Serializer::getSerializer()->serializeObject("Config", *this);
    }

    template <typename Type>
    void Configuration::set(const char* name, const Type& value)
    {
        elementMap[std::string(name)] = ElementType(value);

        // Save the current configuration file inside the serializer
        Serializer::getSerializer()->serializeObject("Config", *this);
    }

    template <>
    void serialize(Archive& archive, const Configuration& config);

    template <>
    Configuration deserialize(const UnserializedObject& serializedString);
}