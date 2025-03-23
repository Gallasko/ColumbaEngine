#pragma once

#include "ECS/system.h"

#include "Memory/elementtype.h"

namespace pg
{
    struct ThemeInfo
    {
        std::string name;

        std::unordered_map<std::string, ElementType> values;
    };

    struct ThemeSystem : System<StoragePolicy, SaveSys>
    {
        virtual std::string getSystemName() const override { return "ThemeSystem"; }

        ThemeSystem();

        virtual void save(Archive& archive) override;

        virtual void load(const UnserializedObject& serializedString) override;

        ThemeInfo getCurrentTheme()
        { 
            auto it = loadedTheme.find(currentTheme);
            
            if (it != loadedTheme.end())
                return it->second;
            else
                return loadedTheme["default"];
        }

        std::string currentTheme = "default";

        std::unordered_map<std::string, ThemeInfo> loadedTheme;
    };
}
