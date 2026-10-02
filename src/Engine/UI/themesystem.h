#pragma once

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "ECS/entitysystem.h"
#include "Memory/elementtype.h"

#include "UI/theme.h"
#include "UI/ttftext.h"

#include "Components/ThemeComponent.generated.h"

namespace pg
{
    // Ask the theme system to switch; anyone may send it.
    struct SetThemeEvent
    {
        SetThemeEvent(const std::string& theme) : theme(theme) {}

        std::string theme;
    };

    // Sent by the theme system once the current theme has changed. Systems that pick element keys
    // from state (hover, disabled...) re-key on it; the repaint of every themed entity is automatic.
    struct ThemeChangedEvent
    {
        ThemeChangedEvent(const std::string& theme) : theme(theme) {}

        std::string theme;
    };

    /**
     * @brief Paints every entity that carries a ThemeComponent from the current theme.
     *
     * The component holds one string, the element key. The system resolves it in the loaded Theme to a map of
     * role values (color, alpha, font, radius, border) and applies them to the drawables found beside the
     * component. Attach a ThemeComponent (or setElement on one) and the paint happens on its own; setTheme or
     * a SetThemeEvent repaints everything.
     *
     * One ECS group per themable drawable type feeds the apply jobs. The engine drawables are registered at
     * init (the ones whose system exists); a system created later registers its own pairing with
     * registerThemable<Comp>() from its init, as TTFTextSystem does.
     */
    struct ThemeSystem : public System<Own<ThemeComponent>, Listener<SetThemeEvent>, QueuedListener<ThemeComponentChangedEvent>, InitSys, SaveSys>
    {
        using ApplyFn = std::function<void(EntityRef, const ElementMap&, const ThemeSystem&)>;

        virtual std::string getSystemName() const override { return "Theme System"; }

        virtual void init() override;

        virtual void onEvent(const SetThemeEvent& event) override;

        virtual void onProcessEvent(const ThemeComponentChangedEvent& event) override;

        virtual void execute() override;

        virtual void save(Archive& archive) override;

        virtual void load(const UnserializedObject& serializedString) override;

        // Replaces the theme data. Fonts are registered right away when a TTFTextSystem exists, otherwise when
        // it registers itself. Returns theme().ok().
        bool loadTheme(const std::string& path, const std::string& fontRoot = "res/font");

        void setThemeData(const Theme& theme, const std::string& fontRoot = "res/font");

        // Switches now, sends ThemeChangedEvent and repaints every themed entity on the next execute.
        void setTheme(const std::string& id);

        const std::string& currentTheme() const { return currentThemeId; }

        // Token access, resolved for the current theme.
        const Theme& theme() const { return data; }

        constant::Vector4D color(const std::string& token) const { return data.color(token, currentThemeId); }

        float spacing(const std::string& token) const { return data.spacing(token, currentThemeId); }

        float space(int step) const { return data.space(step, currentThemeId); }

        float border(const std::string& token) const { return data.border(token, currentThemeId); }

        float radius(const std::string& token) const { return data.radius(token, currentThemeId); }

        float opacity(const std::string& token) const { return data.opacity(token, currentThemeId); }

        const TextStyle& style(const std::string& styleName) const { return data.style(styleName, currentThemeId); }

        bool hasElement(const std::string& key) const { return data.hasElement(key); }

        // The element resolved for the current theme (cached). An unknown key logs once and resolves empty.
        const ElementMap& element(const std::string& key) const;

        // The text style named by the element's font entry ("body" when it has none).
        const TextStyle& elementStyle(const std::string& key) const;

        // One entry of the resolved element, without a type prefix: elementEntry("panel.ground", "color").
        ElementType elementEntry(const std::string& key, const std::string& name) const { return entry(element(key), "", name); }

        // Registers the pairing ThemeComponent + Comp: the entities carrying both get apply called on arrival,
        // on setElement and on every theme change. Skipped when no system owns Comp yet.
        template <typename Comp>
        void registerThemable(ApplyFn apply);

        // Default pairing: color and alpha through Comp::setColors.
        template <typename Comp>
        void registerThemable();

        // Helpers for apply functions. type is Comp::getType(): "<type>.<key>" overrides "<key>".
        ElementType entry(const ElementMap& map, const std::string& type, const std::string& key) const;

        // False when the element has no color entry. A string is a token name or a "#rrggbb[aa]" literal;
        // the alpha entry (literal or opacity token) scales the w channel.
        bool resolveColor(const ElementMap& map, const std::string& type, constant::Vector4D& out) const;

        float resolveAlpha(const ElementMap& map, const std::string& type) const;

        enum class Scale : uint8_t
        {
            Spacing = 0,
            Border  = 1,
            Radius  = 2,
            Opacity = 3
        };

        // False when the element has no such entry. A number is a literal, a string a scale token.
        bool resolveScale(const ElementMap& map, const std::string& type, const std::string& key, Scale scale, float& out) const;

        void registerFontsInto(TTFTextSystem* ttf);

        void repaintAll();

        void applyNow(_unique_id id);

        // A TTFText entity painted by an element: font and color come from the element, the ThemeComponent is attached.
        CompList<PositionComponent, UiAnchor, ViewportComponent, TTFText> makeText(EntitySystem* ecs, const std::string& element, const std::string& text, float x = 0.0f, float y = 0.0f, float z = 0.0f);

        static void applyTTFText(EntityRef entity, const ElementMap& map, const ThemeSystem& theme);

        bool repaintPending = false;

    private:
        void registerBaseThemables();

        Theme data;
        std::string currentThemeId = "default";
        std::string fontRoot = "res/font";
        bool fontsRegistered = false;

        std::vector<ApplyFn> applies;

        // Entity id -> indexes into applies, one per themable drawable present on it
        std::unordered_map<_unique_id, std::vector<size_t>> jobs;

        mutable std::unordered_map<std::string, ElementMap> cache;
        mutable std::set<std::string> loggedUnknownElement;

        std::set<_unique_id> loggedUnthemable;
    };

    template <typename Comp>
    void ThemeSystem::registerThemable(ApplyFn apply)
    {
        // Skip drawables whose owning system isn't registered: registerGroup would create a flag
        // system for the type, which corrupts the ECS if it happens while running. A type with no
        // owner can't be on any entity anyway, so there is nothing to paint.
        if (not registry->hasTypeId<Comp>())
            return;

        const size_t index = applies.size();
        applies.push_back(apply);

        auto group = registerGroup<ThemeComponent, Comp>();

        group->addOnGroup([this, index](EntityRef entity) {
            auto& fns = jobs[entity->id];

            if (std::find(fns.begin(), fns.end(), index) == fns.end())
                fns.push_back(index);

            loggedUnthemable.erase(entity->id);

            // Paint on arrival, whichever of the two components landed last
            applyNow(entity->id);
        });

        group->removeOfGroup([this, index](EntitySystem*, _unique_id id) {
            auto it = jobs.find(id);

            if (it == jobs.end())
                return;

            auto& fns = it->second;
            fns.erase(std::remove(fns.begin(), fns.end(), index), fns.end());

            if (fns.empty())
            {
                jobs.erase(it);
                loggedUnthemable.erase(id);
            }
        });
    }

    template <typename Comp>
    void ThemeSystem::registerThemable()
    {
        registerThemable<Comp>([](EntityRef entity, const ElementMap& map, const ThemeSystem& theme) {
            constant::Vector4D color;

            if (theme.resolveColor(map, Comp::getType(), color))
                entity->get<Comp>()->setColors(color);
        });
    }
}
