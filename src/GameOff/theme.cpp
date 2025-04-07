#include "theme.h"

namespace pg
{
    ThemeSystem::ThemeSystem()
    {
        ThemeInfo defaultTheme;

        defaultTheme.values["tooltip.width"] = 250.0f;
        defaultTheme.values["tooltip.height"] = 120.0f;

        defaultTheme.values["tooltipBg.r"] = 18.0f;
        defaultTheme.values["tooltipBg.g"] = 18.0f;
        defaultTheme.values["tooltipBg.b"] = 18.0f;
        defaultTheme.values["tooltipBg.a"] = 255.0f;

        defaultTheme.values["tooltipSpacer.r"] = 255.0f;
        defaultTheme.values["tooltipSpacer.g"] = 255.0f;
        defaultTheme.values["tooltipSpacer.b"] = 255.0f;
        defaultTheme.values["tooltipSpacer.a"] = 255.0f;

        defaultTheme.values["tooltipSpacer.ratio"] = 0.8f;

        defaultTheme.values["tooltipCostSpacer.topMargin"] = 5.0f;

        defaultTheme.values["tooltipBgHighlight.r"] = 255.0f;
        defaultTheme.values["tooltipBgHighlight.g"] = 255.0f;
        defaultTheme.values["tooltipBgHighlight.b"] = 255.0f;
        defaultTheme.values["tooltipBgHighlight.a"] = 255.0f;

        defaultTheme.values["hoverClickableNexusButton.r"] = 225.0f;
        defaultTheme.values["hoverClickableNexusButton.g"] = 225.0f;
        defaultTheme.values["hoverClickableNexusButton.b"] = 225.0f;
        defaultTheme.values["hoverClickableNexusButton.a"] = 255.0f;

        defaultTheme.values["clickableNexusButton.r"] = 196.0f;
        defaultTheme.values["clickableNexusButton.g"] = 196.0f;
        defaultTheme.values["clickableNexusButton.b"] = 196.0f;
        defaultTheme.values["clickableNexusButton.a"] = 255.0f;

        defaultTheme.values["activeClickableNexusButton.r"] = 120.0f;
        defaultTheme.values["activeClickableNexusButton.g"] = 180.0f;
        defaultTheme.values["activeClickableNexusButton.b"] = 255.0f;
        defaultTheme.values["activeClickableNexusButton.a"] = 255.0f;

        defaultTheme.values["unclickableNexusButton.r"] = 96.0f;
        defaultTheme.values["unclickableNexusButton.g"] = 96.0f;
        defaultTheme.values["unclickableNexusButton.b"] = 96.0f;
        defaultTheme.values["unclickableNexusButton.a"] = 255.0f;

        defaultTheme.values["activeFocus.r"] = 255.0f;
        defaultTheme.values["activeFocus.g"] = 255.0f;
        defaultTheme.values["activeFocus.b"] = 255.0f;
        defaultTheme.values["activeFocus.a"] = 255.0f;

        defaultTheme.values["nexusbutton.width"] = 160.0f;
        defaultTheme.values["nexusbutton.height"] = 55.0f;

        defaultTheme.values["nexusbutton.font"] = "res/font/Inter/static/Inter_28pt-Light.ttf";
        defaultTheme.values["nexusbutton.scale"] = 0.4f;

        defaultTheme.values["resourcedisplay.font"] = "res/font/Inter/static/Inter_28pt-Light.ttf";
        defaultTheme.values["resourcedisplay.scale"] = 0.4f;

        defaultTheme.values["categoryTitle.font"] = "res/font/Inter/static/Inter_28pt-Light.ttf";
        defaultTheme.values["categoryTitle.scale"] = 0.4f;

        defaultTheme.values["tooltip.topMargin"] = 5.0f;
        defaultTheme.values["tooltip.leftMargin"] = 5.0f;
        defaultTheme.values["tooltip.rightMargin"] = 5.0f;
        defaultTheme.values["tooltip.bottomMargin"] = 5.0f;

        defaultTheme.values["tooltipCostValues.topMargin"] = 5.0f;
        defaultTheme.values["tooltipCostValues.leftMargin"] = 12.0f;
        defaultTheme.values["tooltipCostValues.rightMargin"] = 5.0f;

        defaultTheme.values["tooltipTitle.font"] = "res/font/Inter/static/Inter_28pt-Light.ttf";
        defaultTheme.values["tooltipTitle.scale"] = 0.4f;

        defaultTheme.values["resourcedisplay.font"] = "res/font/Inter/static/Inter_28pt-Light.ttf";
        defaultTheme.values["resourcedisplay.scale"] = 0.4f;

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
