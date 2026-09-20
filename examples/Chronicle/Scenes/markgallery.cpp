#include "markgallery.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/simple2dobject.h"

#include "UI/mark.h"
#include "UI/label.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float CONTENT_Z = 20.0f;
    }

    void MarkGallery::init()
    {
        paintSystem = ecsRef->getSystem<PaintSystem>();

        // Vellum background.
        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, 1320.0f, 860.0f, tokens->colour("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        paintSystem->paint(bg.entity, "vellum");

        const float marginX = tokens->space(7);   // 48

        auto placeEntity = [](EntityRef e, float x, float y)
        {
            auto pos = e->get<PositionComponent>();
            pos->setX(x);
            pos->setY(y);
        };

        auto strip = [this](float x, float y, float w, float h, const std::string& token)
        {
            auto s = makeUiSimple2DShape(ecsRef, Shape2D::Square, w, h, tokens->colour(token));
            s.get<PositionComponent>()->setX(x);
            s.get<PositionComponent>()->setY(y);
            s.get<PositionComponent>()->setZ(CONTENT_Z - 1.0f);
            paintSystem->paint(s.entity, token);
            return s.entity.id;
        };

        auto caption = [this](float x, float y, const std::string& text, const std::string& colour)
        {
            LabelSpec spec; spec.style = "caption"; spec.text = text; spec.colour = colour;
            spec.z = static_cast<int>(CONTENT_Z);
            Label l = makeLabel(ecsRef, *tokens, *styles, spec);
            l.box->get<PositionComponent>()->setX(x);
            l.box->get<PositionComponent>()->setY(y);
            return l;
        };

        // ── Row 1: the whole set at S24 with names beneath ────────────────────
        {
            const float y = 48.0f;
            const float step = px(MarkSize::S24) + tokens->space(4);   // 24 + 16
            float x = marginX;
            for (const auto& name : markNames())
            {
                Mark m = makeMark(ecsRef, *tokens, {name, MarkSize::S24, "ink", static_cast<int>(CONTENT_Z)});
                placeEntity(m.entity, x, y);
                caption(x, y + 28.0f, name, "ink-muted");
                x += step;
                if (x > 1320.0f - marginX)   // wrap the row
                {
                    x = marginX;
                }
            }
        }

        // ── Row 2: the set at S14 on a vellum-worn strip (legibility floor) ────
        {
            const float y = 150.0f;
            const float step = px(MarkSize::S14) + tokens->space(2);   // 14 + 8
            const float rowW = static_cast<float>(markNames().size()) * step + tokens->space(2);
            strip(marginX - 4.0f, y - 4.0f, rowW, px(MarkSize::S14) + 8.0f, "vellum-worn");
            float x = marginX;
            for (const auto& name : markNames())
            {
                Mark m = makeMark(ecsRef, *tokens, {name, MarkSize::S14, "ink", static_cast<int>(CONTENT_Z)});
                placeEntity(m.entity, x, y);
                x += step;
            }
        }

        // ── Row 3: pairs, one per size, with a hair rule under each ────────────
        {
            struct Pair { const char* mark; const char* style; const char* text; const char* colour; };
            const Pair pairs[6] = {
                {"time", "tick", "+3 mo", "ink"},
                {"gold", "body-sm", "412 gold", "ink"},
                {"strength", "body", "Strength", "ink"},
                {"reputation", "figure", "STANDING 3", "ink"},
                {"adventure", "title", "North Forest", "ink"},
                {"seal", "versal", "A", "vermilion"},
            };

            float y = 210.0f;
            for (const auto& p : pairs)
            {
                MarkedLabelSpec spec;
                spec.mark = p.mark;
                spec.label = {p.style, p.text, p.colour};
                spec.z = static_cast<int>(CONTENT_Z);
                MarkedLabel ml = makeMarkedLabel(ecsRef, *tokens, *styles, spec);
                placeEntity(ml.root, marginX, y);

                const float rowH = ml.root->get<PositionComponent>()->height;
                strip(marginX, y + rowH + 4.0f, 320.0f, 1.0f, "rule-hair");
                y += rowH + 16.0f;
            }
        }

        // ── Row 4: status - the mark alone must carry the state ───────────────
        {
            const float x = 520.0f;
            float y = 210.0f;
            struct Status { const char* mark; const char* text; const char* colour; };
            const Status rows[2] = {
                {"check", "Strength 14 of 12", "status-gain"},
                {"cross", "Coin 40 of 60", "status-loss"},
            };

            // Same pair on vellum and on a folio leaf.
            for (int leaf = 0; leaf < 2; ++leaf)
            {
                if (leaf == 1)
                    strip(x - 8.0f, y - 8.0f, 260.0f, 80.0f, "folio");

                for (const auto& r : rows)
                {
                    MarkedLabelSpec spec;
                    spec.mark = r.mark;
                    spec.label = {"body", r.text, r.colour};
                    spec.z = static_cast<int>(CONTENT_Z);
                    MarkedLabel ml = makeMarkedLabel(ecsRef, *tokens, *styles, spec);
                    placeEntity(ml.root, x, y);
                    y += ml.root->get<PositionComponent>()->height + 8.0f;
                }
                y += 16.0f;
            }
        }

        // ── Row 5: the missing-mark case draws "seal" and logs once ───────────
        {
            Mark m = makeMark(ecsRef, *tokens, {"clock", MarkSize::S24, "ink", static_cast<int>(CONTENT_Z)});
            placeEntity(m.entity, 520.0f, 460.0f);
            caption(520.0f, 490.0f, "\"clock\" -> seal", "ink-muted");
        }

        // Footer hint.
        caption(marginX, 820.0f, "T  toggle theme", "ink-muted");

        // T toggles the theme; PaintSystem repaints every painted entity (marks included).
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
