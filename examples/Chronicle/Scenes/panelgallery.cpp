#include "panelgallery.h"

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

#include "UI/panel.h"
#include "UI/label.h"
#include "UI/mark.h"
#include "UI/ornament.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;
    }

    void PanelGallery::init()
    {

        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, PAGE_W, PAGE_H, tokens->colour("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        ecsRef->attach<PaintComponent>(bg.entity, "vellum");

        auto place = [](EntityRef e, float x, float y)
        {
            auto p = e->get<PositionComponent>();
            p->setX(x);
            p->setY(y);
        };

        auto caption = [&](float x, float y, const std::string& text)
        {
            LabelSpec spec; spec.style = "caption"; spec.text = text; spec.colour = "ink-muted"; spec.z = 10;
            place(makeLabel(ecsRef, *tokens, *styles, spec).box, x, y);
        };

        // A wrap body label sized to a panel's inner width.
        auto wrapBody = [&](const Panel& p, const std::string& text)
        {
            LabelSpec spec; spec.style = "body"; spec.text = text; spec.colour = "ink";
            spec.overflow = Overflow::Wrap; spec.width = p.innerWidth(); spec.z = p.spec.contentZ;
            return makeLabel(ecsRef, *tokens, *styles, spec).box;
        };

        // ── Row 1: the three design-system panels, to compare with the preview ──
        {
            const float y = 56.0f;
            const float w = 360.0f;
            const float step = w + tokens->space(5);   // 24

            Panel hair = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Hair, w, "Skills", "study"});
            hair.addChild(ecsRef, wrapBody(hair, "The quiet default. A hairline edge, for lists inside a bigger region."));
            place(hair.root, 48.0f, y);

            PanelSpec rs; rs.frame = PanelFrame::Ruled; rs.width = w; rs.heading = "Parts"; rs.glyph = "strength"; rs.aside = "LEDGER";
            Panel ruled = makePanel(ecsRef, *tokens, *styles, rs);
            ruled.addChild(ecsRef, wrapBody(ruled, "The working panel of the game: a drawn rule around a leaf of folio."));
            place(ruled.root, 48.0f + step, y);

            Panel illum = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Illuminated, w, "A relic is found", "relic"});
            illum.addChild(ecsRef, wrapBody(illum, "Gold corners and a doubled frame. Five events in the whole game may use this."));
            place(illum.root, 48.0f + 2.0f * step, y);
        }

        // ── Row 2: engine checks, each under a caption naming what it checks ────
        const float row2Y = 340.0f;
        const float capOffset = 22.0f;

        // Plain, frameless, on bare vellum.
        {
            caption(48.0f, row2Y - capOffset, "plain (no frame)");
            PanelSpec ps; ps.frame = PanelFrame::Plain; ps.width = 300.0f; ps.heading = "Notes";
            Panel plain = makePanel(ecsRef, *tokens, *styles, ps);
            LabelSpec r1; r1.style = "body"; r1.text = "A frameless region"; r1.colour = "ink"; r1.z = ps.contentZ;
            LabelSpec r2; r2.style = "body"; r2.text = "on bare vellum."; r2.colour = "ink"; r2.z = ps.contentZ;
            plain.addChild(ecsRef, makeLabel(ecsRef, *tokens, *styles, r1).box);
            plain.addChild(ecsRef, makeLabel(ecsRef, *tokens, *styles, r2).box);
            place(plain.root, 48.0f, row2Y);
        }

        // Ruled panel holding a ruled list (labels + knotless hair dividers, ground folio).
        {
            const float x = 372.0f;
            caption(x, row2Y - capOffset, "ruled list body");
            Panel p = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Ruled, 300.0f, "Parts", "strength"});
            const char* rows[5] = {"Strength", "Dexterity", "Intelligence", "Vitality", "Reputation"};
            for (int i = 0; i < 5; ++i)
            {
                LabelSpec rl; rl.style = "body"; rl.text = rows[i]; rl.colour = "ink"; rl.z = p.spec.contentZ;
                p.addChild(ecsRef, makeLabel(ecsRef, *tokens, *styles, rl).box);
                if (i < 4)
                {
                    OrnamentSpec ds; ds.kind = OrnamentKind::Divider; ds.weight = DividerWeight::Hair;
                    ds.knot = false; ds.width = p.innerWidth(); ds.ground = "folio"; ds.z = p.spec.contentZ;
                    p.addChild(ecsRef, makeOrnament(ecsRef, *tokens, *styles, ds).root);
                }
            }
            place(p.root, x, row2Y);
        }

        // Long heading + aside: the ellipsis case.
        {
            const float x = 696.0f;
            caption(x, row2Y - capOffset, "ellipsis heading");
            PanelSpec es; es.frame = PanelFrame::Ruled; es.width = 300.0f;
            es.heading = "What Aldren may do this season and the next"; es.glyph = "adventure"; es.aside = "RUNNING";
            Panel p = makePanel(ecsRef, *tokens, *styles, es);
            p.addChild(ecsRef, wrapBody(p, "The title ellipsises; the aside keeps its room."));
            place(p.root, x, row2Y);
        }

        // Nested panel: the z-band case.
        {
            const float x = 1020.0f;
            caption(x, row2Y - capOffset, "nested (z bands)");
            Panel outer = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Ruled, 260.0f, "Who he is"});
            PanelSpec is; is.frame = PanelFrame::Ruled; is.width = outer.innerWidth(); is.heading = "Skills";
            is.z = 20; is.contentZ = 30;
            Panel inner = makePanel(ecsRef, *tokens, *styles, is);
            LabelSpec il; il.style = "body"; il.text = "Swordsmanship"; il.colour = "ink"; il.z = is.contentZ;
            inner.addChild(ecsRef, makeLabel(ecsRef, *tokens, *styles, il).box);
            outer.addChild(ecsRef, inner.root);
            place(outer.root, x, row2Y);
        }

        // Marks in a panel.
        {
            const float x = 48.0f;
            const float y = 620.0f;
            caption(x, y - capOffset, "marks in a panel");
            Panel p = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Ruled, 300.0f, "Season"});
            struct Row { const char* mark; const char* text; const char* colour; };
            const Row rows[3] = {
                {"time", "+3 mo", "ink"},
                {"gold", "412", "ink"},
                {"check", "Strength 14 of 12", "status-gain"},
            };
            for (const auto& r : rows)
            {
                MarkedLabelSpec ms; ms.mark = r.mark; ms.label = {"body", r.text, r.colour}; ms.z = p.spec.contentZ;
                p.addChild(ecsRef, makeMarkedLabel(ecsRef, *tokens, *styles, ms).root);
            }
            place(p.root, x, y);
        }

        caption(48.0f, 820.0f, "T  toggle theme");

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
