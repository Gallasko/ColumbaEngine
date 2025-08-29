#include "thememanager.h"
#include "Files/filemanager.h"
#include "logger.h"
#include <fstream>
#include <sstream>

namespace pg
{
    ThemeManager::ThemeManager()
    {
    }

    void ThemeManager::init()
    {
        LOG_THIS_MEMBER("ThemeManager");
        registerDefaultProperties();
    }

    void ThemeManager::registerDefaultProperties()
    {
        Theme defaultTheme;
        defaultTheme.name = "default";
        defaultTheme.description = "Default engine theme";
        defaultTheme.version = "1.0";

        // Use a simple map like GameOff did, we can convert later if needed
        std::unordered_map<std::string, ElementType>& values = defaultTheme.properties;

        // UI Components - Tooltip
        values["tooltip.width"] = 250.0f;
        values["tooltip.height"] = 120.0f;
        values["tooltip.topMargin"] = 5.0f;
        values["tooltip.leftMargin"] = 5.0f;
        values["tooltip.rightMargin"] = 5.0f;
        values["tooltip.bottomMargin"] = 5.0f;

        // Colors - Tooltip
        values["tooltipBg.r"] = 18.0f;
        values["tooltipBg.g"] = 18.0f;
        values["tooltipBg.b"] = 18.0f;
        values["tooltipBg.a"] = 255.0f;

        values["tooltipBgHighlight.r"] = 255.0f;
        values["tooltipBgHighlight.g"] = 255.0f;
        values["tooltipBgHighlight.b"] = 255.0f;
        values["tooltipBgHighlight.a"] = 255.0f;

        // Spacer elements
        values["tooltipSpacer.r"] = 255.0f;
        values["tooltipSpacer.g"] = 255.0f;
        values["tooltipSpacer.b"] = 255.0f;
        values["tooltipSpacer.a"] = 255.0f;
        values["tooltipSpacer.ratio"] = 0.8f;

        values["tooltipCostSpacer.topMargin"] = 5.0f;

        // Button colors
        values["hoverClickableNexusButton.r"] = 225.0f;
        values["hoverClickableNexusButton.g"] = 225.0f;
        values["hoverClickableNexusButton.b"] = 225.0f;
        values["hoverClickableNexusButton.a"] = 255.0f;

        values["clickableNexusButton.r"] = 196.0f;
        values["clickableNexusButton.g"] = 196.0f;
        values["clickableNexusButton.b"] = 196.0f;
        values["clickableNexusButton.a"] = 255.0f;

        values["activeClickableNexusButton.r"] = 120.0f;
        values["activeClickableNexusButton.g"] = 180.0f;
        values["activeClickableNexusButton.b"] = 255.0f;
        values["activeClickableNexusButton.a"] = 255.0f;

        values["unclickableNexusButton.r"] = 96.0f;
        values["unclickableNexusButton.g"] = 96.0f;
        values["unclickableNexusButton.b"] = 96.0f;
        values["unclickableNexusButton.a"] = 255.0f;

        // Focus colors
        values["activeFocus.r"] = 255.0f;
        values["activeFocus.g"] = 255.0f;
        values["activeFocus.b"] = 255.0f;
        values["activeFocus.a"] = 255.0f;

        // UI Elements - Buttons
        values["nexusbutton.width"] = 185.0f;
        values["nexusbutton.height"] = 45.0f;
        values["nexusbutton.font"] = std::string("res/font/Inter/static/Inter_28pt-Light.ttf");
        values["nexusbutton.scale"] = 0.4f;

        // Typography
        values["resourcedisplay.font"] = std::string("res/font/Inter/static/Inter_28pt-Light.ttf");
        values["resourcedisplay.scale"] = 0.4f;

        values["categoryTitle.font"] = std::string("res/font/Inter/static/Inter_28pt-Light.ttf");
        values["categoryTitle.scale"] = 0.4f;

        values["tooltipTitle.font"] = std::string("res/font/Inter/static/Inter_28pt-Light.ttf");
        values["tooltipTitle.scale"] = 0.4f;

        // Cost values margins
        values["tooltipCostValues.topMargin"] = 5.0f;
        values["tooltipCostValues.leftMargin"] = 12.0f;
        values["tooltipCostValues.rightMargin"] = 5.0f;

        loadedThemes["default"] = defaultTheme;
    }

    // File I/O functions moved to theme module functions

    bool ThemeManager::createTheme(const std::string& name, const std::string& basedOn)
    {
        if (loadedThemes.find(name) != loadedThemes.end())
        {
            LOG_WARNING("ThemeManager", "Theme already exists: " + name);
            return false;
        }

        Theme newTheme;

        if (basedOn != "empty" && loadedThemes.find(basedOn) != loadedThemes.end())
        {
            // Copy from base theme
            newTheme = loadedThemes[basedOn];
            newTheme.name = name;
            newTheme.description = "Custom theme based on " + basedOn;
        }
        else
        {
            // Create empty theme
            newTheme.name = name;
            newTheme.description = "Custom theme";
            newTheme.version = "1.0";
        }

        loadedThemes[name] = newTheme;
        LOG_INFO("ThemeManager", "Created new theme: " + name);
        return true;
    }

    bool ThemeManager::deleteTheme(const std::string& name)
    {
        if (name == "default")
        {
            LOG_ERROR("ThemeManager", "Cannot delete default theme");
            return false;
        }

        auto it = loadedThemes.find(name);
        if (it == loadedThemes.end())
        {
            LOG_WARNING("ThemeManager", "Theme not found: " + name);
            return false;
        }

        if (currentThemeName == name)
        {
            currentThemeName = "default";
        }

        loadedThemes.erase(it);
        LOG_INFO("ThemeManager", "Deleted theme: " + name);
        return true;
    }

    bool ThemeManager::duplicateTheme(const std::string& source, const std::string& newName)
    {
        auto it = loadedThemes.find(source);
        if (it == loadedThemes.end())
        {
            LOG_ERROR("ThemeManager", "Source theme not found: " + source);
            return false;
        }

        if (loadedThemes.find(newName) != loadedThemes.end())
        {
            LOG_WARNING("ThemeManager", "Theme already exists: " + newName);
            return false;
        }

        Theme newTheme = it->second;
        newTheme.name = newName;
        newTheme.description = "Copy of " + source;

        loadedThemes[newName] = newTheme;
        LOG_INFO("ThemeManager", "Duplicated theme " + source + " to " + newName);
        return true;
    }

    void ThemeManager::patchTheme(const std::string& themeName, const std::unordered_map<std::string, ElementType>& patches)
    {
        auto it = loadedThemes.find(themeName);
        if (it == loadedThemes.end())
        {
            LOG_ERROR("ThemeManager", "Theme not found for patching: " + themeName);
            return;
        }

        Theme& theme = it->second;

        for (const auto& [key, value] : patches)
        {
            theme.properties[key] = value;
        }

        LOG_INFO("ThemeManager", "Applied " + std::to_string(patches.size()) + " patches to theme: " + themeName);
    }

    void ThemeManager::save(Archive& archive)
    {
        serialize(archive, "currentTheme", currentThemeName);
        serialize(archive, "loadedThemes", loadedThemes);
    }

    void ThemeManager::load(const UnserializedObject& serializedString)
    {
        defaultDeserialize(serializedString, "currentTheme", currentThemeName);
        defaultDeserialize(serializedString, "loadedThemes", loadedThemes);
    }

    // ThemeProperty not needed anymore with simplified approach

    // Theme serialization
    template<>
    void serialize(Archive& archive, const Theme& theme)
    {
        archive.startSerialization("Theme");
        serialize(archive, "name", theme.name);
        serialize(archive, "description", theme.description);
        serialize(archive, "version", theme.version);
        serialize(archive, "properties", theme.properties);
        archive.endSerialization();
    }

    template<>
    Theme deserialize(const UnserializedObject& serializedString)
    {
        Theme theme;
        defaultDeserialize(serializedString, "name", theme.name);
        defaultDeserialize(serializedString, "description", theme.description);
        defaultDeserialize(serializedString, "version", theme.version);
        defaultDeserialize(serializedString, "properties", theme.properties);
        return theme;
    }

} // namespace pg