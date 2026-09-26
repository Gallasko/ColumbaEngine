#pragma once

#include <string>

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"
#include "UI/themesystem.h"

namespace pg
{
    // The editor's panels are drawn on this atlas texture in every theme.
    static const std::string EditorPanelTexture = "TabTexture";

    // Themed builders: the drawable plus a ThemeComponent keyed by an element of res/editor/theme.json,
    // so the theme system paints it now and repaints it on every theme switch.
    template <typename Type>
    auto makeThemedUiShape(Type* ecsRef, ThemeSystem*, Shape2D shape, float width, float height, const std::string& element)
    {
        auto comp = makeUiSimple2DShape(ecsRef, shape, width, height);

        ecsRef->template attach<ThemeComponent>(comp.entity, element);

        return comp;
    }

    template <typename Type>
    auto makeThemedTTFText(Type* ecsRef, ThemeSystem*, float x, float y, float z, const std::string& font, const std::string& text, float scale, const std::string& element)
    {
        auto comp = makeTTFText(ecsRef, x, y, z, font, text, scale);

        ecsRef->template attach<ThemeComponent>(comp.entity, element);

        return comp;
    }

    template <typename Type>
    auto makeEditorPanel(Type* ecsRef, ThemeSystem*, float width, float height)
    {
        return makeUiTexture(ecsRef, width, height, EditorPanelTexture);
    }

    template <typename Type>
    auto makeEditorButton(Type* ecsRef, ThemeSystem* theme, float width, float height)
    {
        return makeThemedUiShape(ecsRef, theme, Shape2D::Square, width, height, "editor.button");
    }

    template <typename Type>
    auto makeEditorText(Type* ecsRef, ThemeSystem* theme, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, theme, x, y, z, font, text, scale, "editor.text");
    }

    template <typename Type>
    auto makeEditorSecondaryText(Type* ecsRef, ThemeSystem* theme, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, theme, x, y, z, font, text, scale, "editor.text.secondary");
    }

    template <typename Type>
    auto makeEditorHeaderText(Type* ecsRef, ThemeSystem* theme, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, theme, x, y, z, font, text, scale, "editor.text.header");
    }

    template <typename Type>
    auto makeEditorAccentText(Type* ecsRef, ThemeSystem* theme, float x, float y, float z, const std::string& font, const std::string& text, float scale)
    {
        return makeThemedTTFText(ecsRef, theme, x, y, z, font, text, scale, "editor.text.accent");
    }

    template <typename Type>
    auto makeEditorInputBackground(Type* ecsRef, ThemeSystem* theme, float width, float height)
    {
        return makeThemedUiShape(ecsRef, theme, Shape2D::Square, width, height, "editor.input.background");
    }

    template <typename Type>
    auto makeEditorMenuItem(Type* ecsRef, ThemeSystem* theme, float width, float height)
    {
        return makeThemedUiShape(ecsRef, theme, Shape2D::Square, width, height, "editor.menu.item");
    }

    template <typename Type>
    auto makeEditorMenuBackground(Type* ecsRef, ThemeSystem* theme, float width, float height)
    {
        return makeThemedUiShape(ecsRef, theme, Shape2D::Square, width, height, "editor.menu.background");
    }
}
