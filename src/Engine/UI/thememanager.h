#pragma once

#include "ECS/system.h"
#include "Memory/elementtype.h"

namespace pg
{
    struct ThemeProperty
    {
        std::string key;
        ElementType value;
        std::string description;
        std::string category;
        
        ThemeProperty() = default;
        ThemeProperty(const std::string& k, const ElementType& v, const std::string& desc = "", const std::string& cat = "")
            : key(k), value(v), description(desc), category(cat) {}
    };

    struct Theme
    {
        std::string name;
        std::string description;
        std::string version;
        std::unordered_map<std::string, ElementType> properties;
        
        ElementType getValue(const std::string& key) const
        {
            auto it = properties.find(key);
            return it != properties.end() ? it->second : ElementType{};
        }
        
        bool hasProperty(const std::string& key) const
        {
            return properties.find(key) != properties.end();
        }
        
        void setProperty(const std::string& key, const ElementType& value)
        {
            properties[key] = value;
        }
    };

    struct ThemeManager : System<InitSys, SaveSys>
    {
        virtual std::string getSystemName() const override { return "ThemeManager"; }

        ThemeManager();

        virtual void init() override;
        virtual void save(Archive& archive) override;
        virtual void load(const UnserializedObject& serializedString) override;

        const Theme& getCurrentTheme() const
        {
            auto it = loadedThemes.find(currentThemeName);
            if (it != loadedThemes.end())
                return it->second;
            return loadedThemes.at("default");
        }
        
        Theme& getCurrentThemeRef()
        {
            auto it = loadedThemes.find(currentThemeName);
            if (it != loadedThemes.end())
                return it->second;
            return loadedThemes.at("default");
        }

        // File I/O operations moved to separate functions in theme module
        
        bool createTheme(const std::string& name, const std::string& basedOn = "default");
        bool deleteTheme(const std::string& name);
        bool duplicateTheme(const std::string& source, const std::string& newName);
        
        void setCurrentTheme(const std::string& name)
        {
            if (loadedThemes.find(name) != loadedThemes.end())
                currentThemeName = name;
        }
        
        std::vector<std::string> getAvailableThemeNames() const
        {
            std::vector<std::string> names;
            for (const auto& [name, theme] : loadedThemes)
                names.push_back(name);
            return names;
        }
        
        bool hasTheme(const std::string& name) const
        {
            return loadedThemes.find(name) != loadedThemes.end();
        }
        
        void patchTheme(const std::string& themeName, const std::unordered_map<std::string, ElementType>& patches);
        
        void registerDefaultProperties();

        std::string currentThemeName = "default";
        std::unordered_map<std::string, Theme> loadedThemes;
    };

    // Helper functions for theme access
    inline ElementType getThemeValue(const ThemeManager* manager, const std::string& key)
    {
        if (manager)
            return manager->getCurrentTheme().getValue(key);
        return ElementType{};
    }
    
    inline float getThemeFloat(const ThemeManager* manager, const std::string& key, float defaultValue = 0.0f)
    {
        if (manager)
        {
            auto value = manager->getCurrentTheme().getValue(key);
            if (value.type == ElementType::UnionType::FLOAT)
                return value.get<float>();
        }
        return defaultValue;
    }
    
    inline std::string getThemeString(const ThemeManager* manager, const std::string& key, const std::string& defaultValue = "")
    {
        if (manager)
        {
            auto value = manager->getCurrentTheme().getValue(key);
            if (value.type == ElementType::UnionType::STRING)
                return value.get<std::string>();
        }
        return defaultValue;
    }

    // Forward declarations for serialization
    template<>
    void serialize(Archive& archive, const Theme& theme);

    template<>
    Theme deserialize(const UnserializedObject& serializedString);
}