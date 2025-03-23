#include "theme.h"

namespace pg
{
    ThemeSystem::ThemeSystem()
    {
        ThemeInfo defaultTheme;

        defaultTheme.values["tooltipBg.r"] = 18.0f;
        defaultTheme.values["tooltipBg.g"] = 18.0f;
        defaultTheme.values["tooltipBg.b"] = 18.0f;
        defaultTheme.values["tooltipBg.a"] = 255.0f;

        defaultTheme.values["hoverClickableNexusButton.r"] = 225.0f;
        defaultTheme.values["hoverClickableNexusButton.g"] = 225.0f;
        defaultTheme.values["hoverClickableNexusButton.b"] = 225.0f;
        defaultTheme.values["hoverClickableNexusButton.a"] = 255.0f;

        defaultTheme.values["clickableNexusButton.r"] = 196.0f;
        defaultTheme.values["clickableNexusButton.g"] = 196.0f;
        defaultTheme.values["clickableNexusButton.b"] = 196.0f;
        defaultTheme.values["clickableNexusButton.a"] = 255.0f;
        
        defaultTheme.values["unclickableNexusButton.r"] = 96.0f;
        defaultTheme.values["unclickableNexusButton.g"] = 96.0f;
        defaultTheme.values["unclickableNexusButton.b"] = 96.0f;
        defaultTheme.values["unclickableNexusButton.a"] = 255.0f;

        loadedTheme["default"] = defaultTheme;
    }

    void ThemeSystem::save(Archive& archive)
    {
        serialize(archive, "currentTheme", currentTheme);
    }

    void ThemeSystem::load(const UnserializedObject& serializedString)
    {
        defaultDeserialize(serializedString, "currentTheme", currentTheme);
    }

} // namespace pg
