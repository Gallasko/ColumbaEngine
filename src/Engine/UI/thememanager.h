#pragma once

#include "ECS/system.h"
#include "Memory/elementtype.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

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

    // Theme-aware UI creation helpers
    template<typename T>
    inline auto makeThemedUiShape(T* ecsRef, const ThemeManager* themeManager, Shape2D shape, float width, float height, const std::string& colorKey)
    {
        float r = getThemeFloat(themeManager, colorKey + ".r", 128.0f);
        float g = getThemeFloat(themeManager, colorKey + ".g", 128.0f);
        float b = getThemeFloat(themeManager, colorKey + ".b", 128.0f);
        float a = getThemeFloat(themeManager, colorKey + ".a", 255.0f);
        return makeUiSimple2DShape(ecsRef, shape, width, height, {r, g, b, a});
    }

    template<typename T>
    inline auto makeThemedUiTexture(T* ecsRef, const ThemeManager* themeManager, float width, float height, const std::string& textureKey)
    {
        std::string textureName = getThemeString(themeManager, textureKey, "TabTexture");
        return makeUiTexture(ecsRef, width, height, textureName);
    }

    template<typename T>
    inline auto makeThemedTTFText(T* ecsRef, const ThemeManager* themeManager, float x, float y, float z, const std::string& font, const std::string& text, float scale, const std::string& colorKey)
    {
        float r = getThemeFloat(themeManager, colorKey + ".r", 255.0f);
        float g = getThemeFloat(themeManager, colorKey + ".g", 255.0f);
        float b = getThemeFloat(themeManager, colorKey + ".b", 255.0f);
        float a = getThemeFloat(themeManager, colorKey + ".a", 255.0f);
        return makeTTFText(ecsRef, x, y, z, font, text, scale, {r, g, b, a});
    }

    // Editor-specific themed UI helpers
    template<typename T>
    inline auto makeEditorPanel(T* ecsRef, const ThemeManager* themeManager, float width, float height)
    {
        return makeThemedUiTexture(ecsRef, themeManager, width, height, "editor.panel.texture");
    }

    template<typename T>
    inline auto makeEditorButton(T* ecsRef, const ThemeManager* themeManager, float width, float height)
    {
        return makeThemedUiShape(ecsRef, themeManager, Shape2D::Square, width, height, "editor.button.normal");
    }

    template<typename T>
    inline auto makeEditorText(T* ecsRef, const ThemeManager* themeManager, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, themeManager, x, y, z, font, text, scale, "editor.text.primary");
    }

    template<typename T>
    inline auto makeEditorSecondaryText(T* ecsRef, const ThemeManager* themeManager, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, themeManager, x, y, z, font, text, scale, "editor.text.secondary");
    }

    template<typename T>
    inline auto makeEditorHeaderText(T* ecsRef, const ThemeManager* themeManager, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, themeManager, x, y, z, font, text, scale, "editor.text.header");
    }

    template<typename T>
    inline auto makeEditorAccentText(T* ecsRef, const ThemeManager* themeManager, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, themeManager, x, y, z, font, text, scale, "editor.text.accent");
    }

    template<typename T>
    inline auto makeEditorInputBackground(T* ecsRef, const ThemeManager* themeManager, float width, float height)
    {
        return makeThemedUiShape(ecsRef, themeManager, Shape2D::Square, width, height, "editor.input.background");
    }

    template<typename T>
    inline auto makeEditorMenuItem(T* ecsRef, const ThemeManager* themeManager, float width, float height)
    {
        return makeThemedUiShape(ecsRef, themeManager, Shape2D::Square, width, height, "editor.menu.item");
    }

    template<typename T>
    inline auto makeEditorMenuBackground(T* ecsRef, const ThemeManager* themeManager, float width, float height)
    {
        return makeThemedUiShape(ecsRef, themeManager, Shape2D::Square, width, height, "editor.menu.background");
    }

    // Forward declarations for serialization
    template<>
    void serialize(Archive& archive, const Theme& theme);

    template<>
    Theme deserialize(const UnserializedObject& serializedString);
}