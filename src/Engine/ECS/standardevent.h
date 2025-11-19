#pragma once

#include <unordered_map>

#include "Memory/elementtype.h"

namespace pg
{
    struct StandardEvent
    {
        StandardEvent(const std::string& name = "Noop") : name(name) {}

        StandardEvent(const std::string& name, const std::string& valueName, const ElementType& value) : name(name)
        {
            values[valueName] = value;
        }

        template <typename... Args>
        StandardEvent(const std::string& name, const std::string& valueName, const ElementType& value, const Args&... args) : StandardEvent(name, args...)
        {
            values[valueName] = value;
        }

        template <typename Type>
        StandardEvent(const std::string& name, const std::string& valueName, const Type& value) : name(name)
        {
            values[valueName] = ElementType{value};
        }

        template <typename Type, typename... Args>
        StandardEvent(const std::string& name, const std::string& valueName, const Type& value, const Args&... args) : StandardEvent(name, args...)
        {
            values[valueName] = ElementType{value};
        }

        // Todo need to make a ElementType ctor to avoid a copy

        StandardEvent(const StandardEvent& other) : name(other.name), values(other.values) {}

        StandardEvent& operator=(const StandardEvent& other)
        {
            name = other.name;
            values = other.values;

            return *this;
        }

        bool has(const std::string& valueName) const
        {
            return values.find(valueName) != values.end();
        }

        template <typename T>
        T get(const std::string& valueName) const
        {
            return values.at(valueName).get<T>();
        }

        ElementType getElement(const std::string& valueName) const
        {
            return values.at(valueName);
        }

        std::string name;

        ElementMap values;
    };
}