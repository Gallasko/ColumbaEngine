#include "stdafx.h"

#include "themesystem.h"

#include "logger.h"

#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "2D/texture.h"
#include "UI/iconsystem.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Theme System";

        const ElementMap emptyElement{};

        bool isColorLiteral(const std::string& text)
        {
            return not text.empty() and (text[0] == '#' or text.rfind("rgba(", 0) == 0);
        }
    }

    void ThemeSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        registerBaseThemables();
    }

    void ThemeSystem::registerBaseThemables()
    {
        registerThemable<Simple2DObject>();
        registerThemable<HatchRect2DObject>();
        registerThemable<DottedLine2DObject>();
        registerThemable<IconComponent>();

        registerThemable<RoundedRect2DObject>([](EntityRef entity, const ElementMap& map, const ThemeSystem& theme) {
            auto obj = entity->get<RoundedRect2DObject>();
            constant::Vector4D color;
            float radius = 0.0f;

            if (theme.resolveColor(map, RoundedRect2DObject::getType(), color))
                obj->setColors(color);

            if (theme.resolveScale(map, RoundedRect2DObject::getType(), "radius", Scale::Radius, radius))
                obj->setCornerRadius(radius);
        });

        registerThemable<StrokeRect2DObject>([](EntityRef entity, const ElementMap& map, const ThemeSystem& theme) {
            auto obj = entity->get<StrokeRect2DObject>();
            constant::Vector4D color;
            float value = 0.0f;

            if (theme.resolveColor(map, StrokeRect2DObject::getType(), color))
                obj->setColors(color);

            if (theme.resolveScale(map, StrokeRect2DObject::getType(), "radius", Scale::Radius, value))
                obj->setCornerRadius(value);

            if (theme.resolveScale(map, StrokeRect2DObject::getType(), "border", Scale::Border, value))
                obj->setStrokeWidth(value);
        });

        registerThemable<Texture2DComponent>([](EntityRef entity, const ElementMap& map, const ThemeSystem& theme) {
            if (not theme.entry(map, Texture2DComponent::getType(), "alpha").isEmpty())
                entity->get<Texture2DComponent>()->setOpacity(theme.resolveAlpha(map, Texture2DComponent::getType()));
        });

        // Present when the app created its TTFTextSystem before this system (test fixtures); otherwise
        // TTFTextSystem::init registers the pairing itself.
        registerThemable<TTFText>(&ThemeSystem::applyTTFText);
    }

    void ThemeSystem::applyTTFText(EntityRef entity, const ElementMap& map, const ThemeSystem& theme)
    {
        auto text = entity->get<TTFText>();
        constant::Vector4D color;

        if (theme.resolveColor(map, TTFText::getType(), color))
            text->setColors(color);

        const ElementType font = theme.entry(map, TTFText::getType(), "font");

        if (font.isLitteral())
        {
            const TextStyle& style = theme.style(font.get<std::string>());

            // A pure colour switch must not rebuild the glyphs: only re-alias when the face differs.
            if (text->fontPath != style.fontAlias)
                text->setFontPath(style.fontAlias);

            text->setLetterSpacing(style.letterSpacingPx);
            text->setSpacing(style.lineSpacingPx);
        }
    }

    void ThemeSystem::onEvent(const SetThemeEvent& event)
    {
        setTheme(event.theme);
    }

    void ThemeSystem::onProcessEvent(const ThemeComponentChangedEvent& event)
    {
        applyNow(event.id);
    }

    void ThemeSystem::execute()
    {
        if (not repaintPending)
            return;

        repaintPending = false;

        repaintAll();
    }

    void ThemeSystem::save(Archive& archive)
    {
        serialize(archive, "currentTheme", currentThemeId);
    }

    void ThemeSystem::load(const UnserializedObject& serializedString)
    {
        std::string saved;

        defaultDeserialize(serializedString, "currentTheme", saved);

        if (not saved.empty() and data.hasThemeId(saved))
            setTheme(saved);
    }

    bool ThemeSystem::loadTheme(const std::string& path, const std::string& root)
    {
        Theme theme = Theme::load(path);

        for (const auto& error : theme.errors())
            LOG_ERROR(DOM, error);

        setThemeData(theme, root);

        return data.ok();
    }

    void ThemeSystem::setThemeData(const Theme& theme, const std::string& root)
    {
        data = theme;
        fontRoot = root;
        fontsRegistered = false;
        cache.clear();
        loggedUnknownElement.clear();

        if (not data.hasThemeId(currentThemeId))
            currentThemeId = data.themeIds().front();

        if (ecsRef)
            registerFontsInto(ecsRef->getSystem<TTFTextSystem>());

        repaintPending = true;
    }

    void ThemeSystem::registerFontsInto(TTFTextSystem* ttf)
    {
        if (not ttf or fontsRegistered)
            return;

        data.registerFonts(ttf, fontRoot);

        fontsRegistered = true;
    }

    void ThemeSystem::setTheme(const std::string& id)
    {
        if (not data.hasThemeId(id))
        {
            LOG_ERROR(DOM, "Unknown theme '" << id << "'");
            return;
        }

        if (id == currentThemeId)
            return;

        currentThemeId = id;
        cache.clear();
        repaintPending = true;

        if (ecsRef)
            ecsRef->sendEvent(ThemeChangedEvent{id});
    }

    const ElementMap& ThemeSystem::element(const std::string& key) const
    {
        auto it = cache.find(key);

        if (it != cache.end())
            return it->second;

        bool found = false;
        ElementMap resolved = data.resolveElement(key, currentThemeId, found);

        if (not found and loggedUnknownElement.insert(key).second)
            LOG_ERROR(DOM, "Unknown theme element '" << key << "'");

        return cache.emplace(key, std::move(resolved)).first->second;
    }

    const TextStyle& ThemeSystem::elementStyle(const std::string& key) const
    {
        const ElementType font = entry(element(key), TTFText::getType(), "font");

        if (font.isLitteral())
            return style(font.get<std::string>());

        return style("body");
    }

    ElementType ThemeSystem::entry(const ElementMap& map, const std::string& type, const std::string& key) const
    {
        auto it = map.find(type + "." + key);

        if (it != map.end())
            return it->second;

        it = map.find(key);

        if (it != map.end())
            return it->second;

        return ElementType();
    }

    float ThemeSystem::resolveAlpha(const ElementMap& map, const std::string& type) const
    {
        const ElementType alpha = entry(map, type, "alpha");

        if (alpha.isEmpty())
            return 1.0f;

        if (alpha.isNumber())
            return alpha.get<float>();

        if (alpha.isLitteral())
            return opacity(alpha.get<std::string>());

        return 1.0f;
    }

    bool ThemeSystem::resolveColor(const ElementMap& map, const std::string& type, constant::Vector4D& out) const
    {
        const ElementType value = entry(map, type, "color");

        if (not value.isLitteral())
            return false;

        const std::string text = value.get<std::string>();

        if (isColorLiteral(text))
        {
            if (not Theme::parseColorLiteral(text, out))
            {
                LOG_ERROR(DOM, "Cannot parse color literal '" << text << "'");
                out = {255.0f, 0.0f, 255.0f, 255.0f};
            }
        }
        else
        {
            out = color(text);
        }

        out.w *= resolveAlpha(map, type);

        return true;
    }

    bool ThemeSystem::resolveScale(const ElementMap& map, const std::string& type, const std::string& key, Scale scale, float& out) const
    {
        const ElementType value = entry(map, type, key);

        if (value.isEmpty())
            return false;

        if (value.isNumber())
        {
            out = value.get<float>();
            return true;
        }

        if (not value.isLitteral())
            return false;

        const std::string token = value.get<std::string>();

        switch (scale)
        {
        case Scale::Spacing:
            out = spacing(token);
            break;

        case Scale::Border:
            out = border(token);
            break;

        case Scale::Radius:
            out = radius(token);
            break;

        case Scale::Opacity:
            out = opacity(token);
            break;
        }

        return true;
    }

    void ThemeSystem::applyNow(_unique_id id)
    {
        auto it = jobs.find(id);

        if (it == jobs.end())
            return;    // No themable drawable yet: the group callback paints on arrival

        auto entity = ecsRef->getEntity(id);

        if (not entity or not entity->has<ThemeComponent>())
            return;

        const ElementMap& map = element(entity->get<ThemeComponent>()->element);

        for (auto index : it->second)
            applies[index](entity, map, *this);
    }

    void ThemeSystem::repaintAll()
    {
        // We own ThemeComponent, so a view over it is the whole working set.
        for (auto comp : view<ThemeComponent>())
        {
            if (jobs.count(comp->entityId))
                applyNow(comp->entityId);
            else if (loggedUnthemable.insert(comp->entityId).second)
                LOG_WARNING(DOM, "Entity " << comp->entityId << " has a ThemeComponent but nothing themable");
        }
    }

    CompList<PositionComponent, UiAnchor, ViewportComponent, TTFText> ThemeSystem::makeText(EntitySystem* ecs, const std::string& key, const std::string& text, float x, float y, float z)
    {
        const TextStyle& textStyle = elementStyle(key);

        auto comp = makeTTFText(ecs, x, y, z, textStyle.fontAlias, text, 1.0f);

        auto ttf = comp.get<TTFText>();
        ttf->setLetterSpacing(textStyle.letterSpacingPx);
        ttf->setSpacing(textStyle.lineSpacingPx);

        ecs->attach<ThemeComponent>(comp.entity, key);

        return comp;
    }
}
