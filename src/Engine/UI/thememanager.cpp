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

        // EDITOR-SPECIFIC THEME PROPERTIES
        // Editor Panel Backgrounds & Colors
        values["editor.panel.background.r"] = 55.0f;
        values["editor.panel.background.g"] = 55.0f;
        values["editor.panel.background.b"] = 55.0f;
        values["editor.panel.background.a"] = 255.0f;

        values["editor.panel.border.r"] = 200.0f;
        values["editor.panel.border.g"] = 200.0f;
        values["editor.panel.border.b"] = 200.0f;
        values["editor.panel.border.a"] = 255.0f;

        // Editor Buttons & Interactive Elements
        values["editor.button.normal.r"] = 200.0f;
        values["editor.button.normal.g"] = 200.0f;
        values["editor.button.normal.b"] = 200.0f;
        values["editor.button.normal.a"] = 255.0f;

        values["editor.button.hover.r"] = 220.0f;
        values["editor.button.hover.g"] = 220.0f;
        values["editor.button.hover.b"] = 220.0f;
        values["editor.button.hover.a"] = 255.0f;

        values["editor.button.pressed.r"] = 180.0f;
        values["editor.button.pressed.g"] = 180.0f;
        values["editor.button.pressed.b"] = 180.0f;
        values["editor.button.pressed.a"] = 255.0f;

        // Editor Text & Typography
        values["editor.text.primary.r"] = 255.0f;
        values["editor.text.primary.g"] = 255.0f;
        values["editor.text.primary.b"] = 255.0f;
        values["editor.text.primary.a"] = 255.0f;

        values["editor.text.secondary.r"] = 200.0f;
        values["editor.text.secondary.g"] = 200.0f;
        values["editor.text.secondary.b"] = 200.0f;
        values["editor.text.secondary.a"] = 255.0f;

        values["editor.text.header.r"] = 255.0f;
        values["editor.text.header.g"] = 255.0f;
        values["editor.text.header.b"] = 255.0f;
        values["editor.text.header.a"] = 255.0f;

        values["editor.text.accent.r"] = 255.0f;
        values["editor.text.accent.g"] = 100.0f;
        values["editor.text.accent.b"] = 100.0f;
        values["editor.text.accent.a"] = 255.0f;

        // Editor List Items
        values["editor.item.background.r"] = 70.0f;
        values["editor.item.background.g"] = 70.0f;
        values["editor.item.background.b"] = 70.0f;
        values["editor.item.background.a"] = 255.0f;

        values["editor.item.selected.r"] = 100.0f;
        values["editor.item.selected.g"] = 150.0f;
        values["editor.item.selected.b"] = 200.0f;
        values["editor.item.selected.a"] = 255.0f;

        values["editor.item.hover.r"] = 90.0f;
        values["editor.item.hover.g"] = 90.0f;
        values["editor.item.hover.b"] = 90.0f;
        values["editor.item.hover.a"] = 255.0f;

        // Editor Input Fields
        values["editor.input.background.r"] = 55.0f;
        values["editor.input.background.g"] = 55.0f;
        values["editor.input.background.b"] = 55.0f;
        values["editor.input.background.a"] = 255.0f;

        values["editor.input.border.r"] = 150.0f;
        values["editor.input.border.g"] = 150.0f;
        values["editor.input.border.b"] = 150.0f;
        values["editor.input.border.a"] = 255.0f;

        values["editor.input.text.r"] = 255.0f;
        values["editor.input.text.g"] = 255.0f;
        values["editor.input.text.b"] = 255.0f;
        values["editor.input.text.a"] = 255.0f;

        // Editor Context Menu
        values["editor.menu.background.r"] = 20.0f;
        values["editor.menu.background.g"] = 20.0f;
        values["editor.menu.background.b"] = 20.0f;
        values["editor.menu.background.a"] = 255.0f;

        values["editor.menu.item.r"] = 40.0f;
        values["editor.menu.item.g"] = 40.0f;
        values["editor.menu.item.b"] = 40.0f;
        values["editor.menu.item.a"] = 255.0f;

        // Editor Texture Names
        values["editor.panel.texture"] = std::string("TabTexture");
        values["editor.input.texture"] = std::string("TabTexture");
        values["editor.button.texture"] = std::string("TabTexture");

        loadedThemes["default"] = defaultTheme;

        // Create white theme
        Theme whiteTheme;
        whiteTheme.name = "white";
        whiteTheme.description = "Clean white theme for editor";
        whiteTheme.version = "1.0";
        std::unordered_map<std::string, ElementType>& whiteValues = whiteTheme.properties;

        // Copy game-specific properties from default theme
        for (const auto& [key, value] : defaultTheme.properties) {
            if (key.find("editor.") != 0) { // Not editor-specific
                whiteValues[key] = value;
            }
        }

        // WHITE THEME - Editor Panel Backgrounds & Colors
        whiteValues["editor.panel.background.r"] = 250.0f;
        whiteValues["editor.panel.background.g"] = 250.0f;
        whiteValues["editor.panel.background.b"] = 250.0f;
        whiteValues["editor.panel.background.a"] = 255.0f;

        whiteValues["editor.panel.border.r"] = 180.0f;
        whiteValues["editor.panel.border.g"] = 180.0f;
        whiteValues["editor.panel.border.b"] = 180.0f;
        whiteValues["editor.panel.border.a"] = 255.0f;

        // WHITE THEME - Editor Buttons & Interactive Elements
        whiteValues["editor.button.normal.r"] = 230.0f;
        whiteValues["editor.button.normal.g"] = 230.0f;
        whiteValues["editor.button.normal.b"] = 230.0f;
        whiteValues["editor.button.normal.a"] = 255.0f;

        whiteValues["editor.button.hover.r"] = 210.0f;
        whiteValues["editor.button.hover.g"] = 210.0f;
        whiteValues["editor.button.hover.b"] = 210.0f;
        whiteValues["editor.button.hover.a"] = 255.0f;

        whiteValues["editor.button.pressed.r"] = 190.0f;
        whiteValues["editor.button.pressed.g"] = 190.0f;
        whiteValues["editor.button.pressed.b"] = 190.0f;
        whiteValues["editor.button.pressed.a"] = 255.0f;

        // WHITE THEME - Editor Text & Typography
        whiteValues["editor.text.primary.r"] = 40.0f;
        whiteValues["editor.text.primary.g"] = 40.0f;
        whiteValues["editor.text.primary.b"] = 40.0f;
        whiteValues["editor.text.primary.a"] = 255.0f;

        whiteValues["editor.text.secondary.r"] = 80.0f;
        whiteValues["editor.text.secondary.g"] = 80.0f;
        whiteValues["editor.text.secondary.b"] = 80.0f;
        whiteValues["editor.text.secondary.a"] = 255.0f;

        whiteValues["editor.text.header.r"] = 20.0f;
        whiteValues["editor.text.header.g"] = 20.0f;
        whiteValues["editor.text.header.b"] = 20.0f;
        whiteValues["editor.text.header.a"] = 255.0f;

        whiteValues["editor.text.accent.r"] = 50.0f;
        whiteValues["editor.text.accent.g"] = 120.0f;
        whiteValues["editor.text.accent.b"] = 200.0f;
        whiteValues["editor.text.accent.a"] = 255.0f;

        // WHITE THEME - Editor List Items
        whiteValues["editor.item.background.r"] = 245.0f;
        whiteValues["editor.item.background.g"] = 245.0f;
        whiteValues["editor.item.background.b"] = 245.0f;
        whiteValues["editor.item.background.a"] = 255.0f;

        whiteValues["editor.item.selected.r"] = 200.0f;
        whiteValues["editor.item.selected.g"] = 220.0f;
        whiteValues["editor.item.selected.b"] = 255.0f;
        whiteValues["editor.item.selected.a"] = 255.0f;

        whiteValues["editor.item.hover.r"] = 235.0f;
        whiteValues["editor.item.hover.g"] = 235.0f;
        whiteValues["editor.item.hover.b"] = 235.0f;
        whiteValues["editor.item.hover.a"] = 255.0f;

        // WHITE THEME - Editor Input Fields
        whiteValues["editor.input.background.r"] = 255.0f;
        whiteValues["editor.input.background.g"] = 255.0f;
        whiteValues["editor.input.background.b"] = 255.0f;
        whiteValues["editor.input.background.a"] = 255.0f;

        whiteValues["editor.input.border.r"] = 200.0f;
        whiteValues["editor.input.border.g"] = 200.0f;
        whiteValues["editor.input.border.b"] = 200.0f;
        whiteValues["editor.input.border.a"] = 255.0f;

        whiteValues["editor.input.text.r"] = 40.0f;
        whiteValues["editor.input.text.g"] = 40.0f;
        whiteValues["editor.input.text.b"] = 40.0f;
        whiteValues["editor.input.text.a"] = 255.0f;

        // WHITE THEME - Editor Context Menu
        whiteValues["editor.menu.background.r"] = 240.0f;
        whiteValues["editor.menu.background.g"] = 240.0f;
        whiteValues["editor.menu.background.b"] = 240.0f;
        whiteValues["editor.menu.background.a"] = 255.0f;

        whiteValues["editor.menu.item.r"] = 220.0f;
        whiteValues["editor.menu.item.g"] = 220.0f;
        whiteValues["editor.menu.item.b"] = 220.0f;
        whiteValues["editor.menu.item.a"] = 255.0f;

        // WHITE THEME - Editor Texture Names
        whiteValues["editor.panel.texture"] = std::string("TabTexture");
        whiteValues["editor.input.texture"] = std::string("TabTexture");
        whiteValues["editor.button.texture"] = std::string("TabTexture");

        loadedThemes["white"] = whiteTheme;

        // Create dark theme
        Theme darkTheme;
        darkTheme.name = "dark";
        darkTheme.description = "Modern dark theme for editor";
        darkTheme.version = "1.0";
        std::unordered_map<std::string, ElementType>& darkValues = darkTheme.properties;

        // Copy game-specific properties from default theme
        for (const auto& [key, value] : defaultTheme.properties) {
            if (key.find("editor.") != 0) { // Not editor-specific
                darkValues[key] = value;
            }
        }

        // DARK THEME - Editor Panel Backgrounds & Colors
        darkValues["editor.panel.background.r"] = 30.0f;
        darkValues["editor.panel.background.g"] = 30.0f;
        darkValues["editor.panel.background.b"] = 30.0f;
        darkValues["editor.panel.background.a"] = 255.0f;

        darkValues["editor.panel.border.r"] = 80.0f;
        darkValues["editor.panel.border.g"] = 80.0f;
        darkValues["editor.panel.border.b"] = 80.0f;
        darkValues["editor.panel.border.a"] = 255.0f;

        // DARK THEME - Editor Buttons & Interactive Elements
        darkValues["editor.button.normal.r"] = 60.0f;
        darkValues["editor.button.normal.g"] = 60.0f;
        darkValues["editor.button.normal.b"] = 60.0f;
        darkValues["editor.button.normal.a"] = 255.0f;

        darkValues["editor.button.hover.r"] = 80.0f;
        darkValues["editor.button.hover.g"] = 80.0f;
        darkValues["editor.button.hover.b"] = 80.0f;
        darkValues["editor.button.hover.a"] = 255.0f;

        darkValues["editor.button.pressed.r"] = 40.0f;
        darkValues["editor.button.pressed.g"] = 40.0f;
        darkValues["editor.button.pressed.b"] = 40.0f;
        darkValues["editor.button.pressed.a"] = 255.0f;

        // DARK THEME - Editor Text & Typography
        darkValues["editor.text.primary.r"] = 220.0f;
        darkValues["editor.text.primary.g"] = 220.0f;
        darkValues["editor.text.primary.b"] = 220.0f;
        darkValues["editor.text.primary.a"] = 255.0f;

        darkValues["editor.text.secondary.r"] = 160.0f;
        darkValues["editor.text.secondary.g"] = 160.0f;
        darkValues["editor.text.secondary.b"] = 160.0f;
        darkValues["editor.text.secondary.a"] = 255.0f;

        darkValues["editor.text.header.r"] = 240.0f;
        darkValues["editor.text.header.g"] = 240.0f;
        darkValues["editor.text.header.b"] = 240.0f;
        darkValues["editor.text.header.a"] = 255.0f;

        darkValues["editor.text.accent.r"] = 100.0f;
        darkValues["editor.text.accent.g"] = 170.0f;
        darkValues["editor.text.accent.b"] = 255.0f;
        darkValues["editor.text.accent.a"] = 255.0f;

        // DARK THEME - Editor List Items
        darkValues["editor.item.background.r"] = 45.0f;
        darkValues["editor.item.background.g"] = 45.0f;
        darkValues["editor.item.background.b"] = 45.0f;
        darkValues["editor.item.background.a"] = 255.0f;

        darkValues["editor.item.selected.r"] = 80.0f;
        darkValues["editor.item.selected.g"] = 120.0f;
        darkValues["editor.item.selected.b"] = 180.0f;
        darkValues["editor.item.selected.a"] = 255.0f;

        darkValues["editor.item.hover.r"] = 65.0f;
        darkValues["editor.item.hover.g"] = 65.0f;
        darkValues["editor.item.hover.b"] = 65.0f;
        darkValues["editor.item.hover.a"] = 255.0f;

        // DARK THEME - Editor Input Fields
        darkValues["editor.input.background.r"] = 25.0f;
        darkValues["editor.input.background.g"] = 25.0f;
        darkValues["editor.input.background.b"] = 25.0f;
        darkValues["editor.input.background.a"] = 255.0f;

        darkValues["editor.input.border.r"] = 100.0f;
        darkValues["editor.input.border.g"] = 100.0f;
        darkValues["editor.input.border.b"] = 100.0f;
        darkValues["editor.input.border.a"] = 255.0f;

        darkValues["editor.input.text.r"] = 200.0f;
        darkValues["editor.input.text.g"] = 200.0f;
        darkValues["editor.input.text.b"] = 200.0f;
        darkValues["editor.input.text.a"] = 255.0f;

        // DARK THEME - Editor Context Menu
        darkValues["editor.menu.background.r"] = 20.0f;
        darkValues["editor.menu.background.g"] = 20.0f;
        darkValues["editor.menu.background.b"] = 20.0f;
        darkValues["editor.menu.background.a"] = 255.0f;

        darkValues["editor.menu.item.r"] = 50.0f;
        darkValues["editor.menu.item.g"] = 50.0f;
        darkValues["editor.menu.item.b"] = 50.0f;
        darkValues["editor.menu.item.a"] = 255.0f;

        // DARK THEME - Editor Texture Names
        darkValues["editor.panel.texture"] = std::string("TabTexture");
        darkValues["editor.input.texture"] = std::string("TabTexture");
        darkValues["editor.button.texture"] = std::string("TabTexture");

        loadedThemes["dark"] = darkTheme;
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