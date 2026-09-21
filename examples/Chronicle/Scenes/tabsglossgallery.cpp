#include "tabsglossgallery.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"

#include "UI/tabs.h"
#include "UI/button.h"
#include "UI/panel.h"
#include "UI/mark.h"
#include "UI/gloss.h"
#include "UI/label.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;
    }

    void TabsGlossGallery::init()
    {
        paintSystem = ecsRef->getSystem<PaintSystem>();
        auto* reg = ecsRef->getSystem<GlossRegistry>();

        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, PAGE_W, PAGE_H, tokens->colour("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        paintSystem->paint(bg.entity, "vellum");

        auto place = [](EntityRef e, float x, float y)
        {
            auto p = e->get<PositionComponent>();
            p->setX(x);
            p->setY(y);
        };

        auto caption = [&](float x, float y, const std::string& text, const std::string& colour)
        {
            LabelSpec spec; spec.style = "caption"; spec.text = text; spec.colour = colour; spec.z = 15;
            place(makeLabel(ecsRef, *tokens, *styles, spec).box, x, y);
        };

        const float margin = tokens->space(7);   // 48

        // ── Register the Parts row glosses (one is deliberately absent) ────────
        if (reg)
        {
            GlossSpec str; str.title = "Strength";
            str.text = "Lifting, striking, enduring. Grows at the yard and in the mines.";
            str.rows = {{"Now", "14"}, {"At term", "17"}, {"Warrior at 18 asks", "18"}};
            str.footnote = "WARRIOR AT 18 ASKS STRENGTH 18";
            reg->set("parts/strength", str);

            GlossSpec dex; dex.title = "Dexterity";
            dex.text = "Speed of hand and foot. The difference between a parry and a wound.";
            dex.rows = {{"Now", "9"}, {"At term", "11"}};
            reg->set("parts/dexterity", dex);
            // "parts/missing" left unregistered on purpose.
        }

        // ── Two buttons, then the tab row: the shared Tab order ───────────────
        makeButton(ecsRef, *tokens, *styles, {ButtonVariant::Quiet, "Back", "", -1, false, "", "back"})
            .root->get<PositionComponent>()->setX(margin);
        {
            Button b = makeButton(ecsRef, *tokens, *styles, {ButtonVariant::Quiet, "Menu", "", -1, false, "", "menu"});
            place(b.root, margin, 44.0f);
        }
        {
            Button b2 = makeButton(ecsRef, *tokens, *styles, {ButtonVariant::Quiet, "Back", "", -1, false, "", "back"});
            place(b2.root, margin + 90.0f, 44.0f);
        }

        TabsSpec ts; ts.tag = "life"; ts.active = 0; ts.width = PAGE_W - 2.0f * margin;   // 1224
        ts.items = {
            {"Life", "quill", 0}, {"Kit", "equipment", 0}, {"Town", "town", 0},
            {"Guild", "guild", 3}, {"Adventure", "adventure", 0}, {"Chronicle", "study", 12}};
        Tabs tabsRow = makeTabs(ecsRef, *tokens, *styles, ts);
        place(tabsRow.root, margin, 96.0f);

        // Echo the last selection.
        LabelSpec ss; ss.style = "body-sm"; ss.text = "selected: Life"; ss.colour = "ink-muted"; ss.z = 15;
        selected = makeLabel(ecsRef, *tokens, *styles, ss);
        place(selected.box, margin, 150.0f);

        const std::vector<TabItem> items = ts.items;
        listenToEvent<TabSelectedEvent>([this, items](const TabSelectedEvent& e)
        {
            if (e.index < 0 or e.index >= static_cast<int>(items.size()))
                return;
            const TabItem& it = items[e.index];
            std::string text = "selected: " + it.label;
            if (it.badge > 0)
                text += " (" + std::to_string(it.badge) + ")";
            selected.setText(ecsRef, *styles, text);
        });

        // ── Left: a Parts panel whose rows carry glosses ──────────────────────
        {
            const float y = 200.0f;
            caption(margin, y - 22.0f, "hover a row for its gloss (200 ms)", "ink-muted");
            Panel parts = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength"});
            struct Row { const char* mark; const char* text; const char* key; };
            const Row rows[3] = {
                {"strength", "Strength 14", "parts/strength"},
                {"dexterity", "Dexterity 9", "parts/dexterity"},
                {"intelligence", "Intellect 11", "parts/missing"},   // unregistered -> vermilion fallback
            };
            for (const auto& r : rows)
            {
                MarkedLabelSpec ms; ms.mark = r.mark; ms.label = {"body", r.text, "ink"}; ms.z = parts.spec.contentZ;
                MarkedLabel ml = makeMarkedLabel(ecsRef, *tokens, *styles, ms);
                parts.addChild(ecsRef, ml.root);
                attachGloss(ecsRef, ml.root, r.key);
            }
            place(parts.root, margin, y);
        }

        // ── Beside it: two margin glosses (240) at 40 and ~80 words ───────────
        {
            const float x = 420.0f;
            caption(x, 178.0f, "margin gloss (40 words)", "ink-muted");
            GlossSpec g1; g1.kind = GlossKind::Margin;
            g1.text = "He had not the strength for it, and the guild would not wait; the smith took him "
                      "on for the winter and asked no questions of the boy at all.";
            place(makeGloss(ecsRef, *tokens, *styles, g1).root, x, 200.0f);

            caption(x, 320.0f, "margin gloss (80 words - past the ceiling)", "ink-muted");
            GlossSpec g2; g2.kind = GlossKind::Margin;
            g2.text = "He had not the strength for it, and the guild would not wait; the smith took him on "
                      "for the winter and asked no questions of the boy at all, and in the spring when the "
                      "roads opened again he found the work had made a wideness in his shoulders and a "
                      "patience in his hands that the yard had never given him, and he wondered whether he "
                      "had chosen the forge or the forge had chosen him.";
            place(makeGloss(ecsRef, *tokens, *styles, g2).root, x, 342.0f);
        }

        // ── Bottom-right: a static tooltip gloss for the design comparison ────
        {
            const float x = 760.0f, y = 200.0f;
            caption(x, y - 22.0f, "tooltip gloss (static)", "ink-muted");
            GlossSpec tip; tip.kind = GlossKind::Tooltip; tip.title = "Strength";
            tip.text = "Lifting, striking, enduring. Grows at the yard and in the mines over a season.";
            tip.rows = {{"Now", "14"}, {"At term", "17"}, {"Warrior at 18 asks", "18"}};
            tip.footnote = "WARRIOR AT 18 ASKS STRENGTH 18";
            place(makeGloss(ecsRef, *tokens, *styles, tip).root, x, y);
        }

        caption(margin, 820.0f,
            "Tab / Shift-Tab \xC2\xB7 <- -> within tabs \xC2\xB7 Enter selects \xC2\xB7 hover a Parts row for its gloss (200 ms)",
            "ink-muted");

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event)
        {
            if (event.key == SDL_SCANCODE_T)
            {
                const Theme next = tokens->theme() == Theme::Day ? Theme::Candle : Theme::Day;
                tokens->setTheme(next);
                ecsRef->sendEvent(ThemeChangedEvent{next});
            }
        });

        listenToEvent<ResizeEvent>([this](const ResizeEvent& event)
        {
            auto entity = ecsRef->getEntity(backgroundId);
            if (entity and entity->has<PositionComponent>())
            {
                entity->get<PositionComponent>()->setWidth(event.width);
                entity->get<PositionComponent>()->setHeight(event.height);
            }
        });
    }
}
