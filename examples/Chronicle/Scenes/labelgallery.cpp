#include "labelgallery.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

#include "UI/label.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float COL_W = 280.0f;
        constexpr float CONTENT_Z = 20.0f;
        const std::string PARA =
            "Lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor "
            "incididunt ut labore et dolore magna aliqua ut enim ad minim veniam quis nostrud "
            "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat duis aute irure";
    }

    void LabelGallery::init()
    {
        auto* ttf = ecsRef->getSystem<TTFTextSystem>();

        // Vellum background.
        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, 1320.0f, 860.0f, tokens->colour("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        ecsRef->attach<PaintComponent>(bg.entity, "vellum");

        const float colX[4] = {48.0f, 376.0f, 704.0f, 1032.0f};

        // Places a label's box at (x, y). Use the stored EntityRef, not getEntity:
        // in a loading scene the entity is freshly created and not yet in the registry.
        auto place = [](Label& l, float x, float y)
        {
            auto pos = l.box->get<PositionComponent>();
            pos->setX(x);
            pos->setY(y);
        };

        // A thin painted rule.
        auto rule = [this](float x, float y, float w, float h, const std::string& token)
        {
            auto r = makeUiSimple2DShape(ecsRef, Shape2D::Square, w, h, tokens->colour(token));
            r.get<PositionComponent>()->setX(x);
            r.get<PositionComponent>()->setY(y);
            r.get<PositionComponent>()->setZ(CONTENT_Z - 1.0f);
            ecsRef->attach<PaintComponent>(r.entity, token);
            return r.entity.id;
        };

        // A filled strip so a box's edges are visible.
        auto strip = [this](float x, float y, float w, float h, const std::string& token)
        {
            auto s = makeUiSimple2DShape(ecsRef, Shape2D::Square, w, h, tokens->colour(token));
            s.get<PositionComponent>()->setX(x);
            s.get<PositionComponent>()->setY(y);
            s.get<PositionComponent>()->setZ(CONTENT_Z - 1.0f);
            ecsRef->attach<PaintComponent>(s.entity, token);
        };

        // Column header: a heading label and a hairline beneath it. Returns the y below the rule.
        auto header = [&](float x, const std::string& title) -> float
        {
            LabelSpec h; h.style = "heading"; h.text = title; h.colour = "ink"; h.z = static_cast<int>(CONTENT_Z);
            Label label = makeLabel(ecsRef, *tokens, *styles, h);
            place(label, x, 40.0f);
            rule(x, 72.0f, COL_W, 1.0f, "rule-hair");
            return 84.0f;
        };

        // ── Column 0: Alignment ───────────────────────────────────────────────
        {
            float y = header(colX[0], "Alignment");
            const Align aligns[3] = {Align::Left, Align::Centre, Align::Right};
            const char* names[3] = {"Left", "Centre", "Right"};
            for (int i = 0; i < 3; ++i)
            {
                strip(colX[0], y, COL_W, 24.0f, "vellum-tint");
                LabelSpec spec; spec.style = "body"; spec.text = names[i]; spec.align = aligns[i];
                spec.overflow = Overflow::Ellipsis; spec.width = COL_W; spec.z = static_cast<int>(CONTENT_Z);
                Label l = makeLabel(ecsRef, *tokens, *styles, spec);
                place(l, colX[0], y);
                y += 32.0f;
            }
            const char* figures[3] = {"412", "1 204", "\xE2\x88\x92" "3 mo"};
            for (int i = 0; i < 3; ++i)
            {
                strip(colX[0], y, COL_W, 22.0f, "vellum-tint");
                LabelSpec spec; spec.style = "figure"; spec.text = figures[i]; spec.align = Align::Right;
                spec.overflow = Overflow::Ellipsis; spec.width = COL_W; spec.colour = "ochre"; spec.z = static_cast<int>(CONTENT_Z);
                Label l = makeLabel(ecsRef, *tokens, *styles, spec);
                place(l, colX[0], y);
                y += 30.0f;
            }
        }

        // ── Column 1: Grow (a tick at each box's right edge) ──────────────────
        {
            float y = header(colX[1], "Grow");
            struct Item { const char* style; const char* text; };
            const Item items[3] = {{"figure", "STR 14"}, {"label", "REQUIRES"}, {"title", "North Forest"}};
            for (const auto& item : items)
            {
                LabelSpec spec; spec.style = item.style; spec.text = item.text; spec.z = static_cast<int>(CONTENT_Z);
                Label l = makeLabel(ecsRef, *tokens, *styles, spec);
                place(l, colX[1], y);

                const TextStyle& st = styles->get(item.style);
                const float w = ttf->measureText(st.fontAlias, item.text, 1.0f, 0.0f, 0.0f, st.letterSpacingPx).width;
                rule(colX[1] + w, y, 1.0f, static_cast<float>(st.lineHeightPx), "rule-ruled");

                y += static_cast<float>(st.lineHeightPx) + 12.0f;
            }
        }

        // ── Column 2: Ellipsis at shrinking widths ────────────────────────────
        {
            float y = header(colX[2], "Ellipsis");
            const float widths[5] = {280.0f, 200.0f, 120.0f, 60.0f, 20.0f};
            for (float w : widths)
            {
                strip(colX[2], y, w, 24.0f, "vellum-tint");
                LabelSpec spec; spec.style = "body"; spec.text = "The quick brown fox jumps over the lazy dog";
                spec.overflow = Overflow::Ellipsis; spec.width = w; spec.z = static_cast<int>(CONTENT_Z);
                Label l = makeLabel(ecsRef, *tokens, *styles, spec);
                place(l, colX[2], y);
                y += 30.0f;
            }
        }

        // ── Column 3: Wrap variants, each annotated with its line count/height ──
        {
            float y = header(colX[3], "Wrap");
            struct Wrap { const char* style; int maxLines; const char* colour; };
            const Wrap variants[4] = {{"body", 0, "ink"}, {"body", 3, "ink"}, {"body", 1, "ink"}, {"body-sm", 2, "ink-muted"}};
            for (const auto& v : variants)
            {
                LabelSpec spec; spec.style = v.style; spec.text = PARA; spec.colour = v.colour;
                spec.overflow = Overflow::Wrap; spec.width = COL_W; spec.maxLines = v.maxLines; spec.z = static_cast<int>(CONTENT_Z);
                Label l = makeLabel(ecsRef, *tokens, *styles, spec);
                place(l, colX[3], y);

                const TextStyle& st = styles->get(v.style);
                const int lines = countLines(*ttf, st, l.fitted, COL_W);
                const float h = static_cast<float>(lines) * static_cast<float>(st.lineHeightPx);

                LabelSpec meta; meta.style = "caption"; meta.colour = "ink-faint";
                meta.text = std::to_string(lines) + " lines \xC2\xB7 " + std::to_string(static_cast<int>(h)) + " px";
                meta.z = static_cast<int>(CONTENT_Z);
                Label metaLabel = makeLabel(ecsRef, *tokens, *styles, meta);
                place(metaLabel, colX[3], y + h + 2.0f);

                y += h + 24.0f;
            }
        }

        // Footer hint.
        LabelSpec hint; hint.style = "caption"; hint.text = "T  toggle theme"; hint.colour = "ink-muted"; hint.z = static_cast<int>(CONTENT_Z);
        Label hintLabel = makeLabel(ecsRef, *tokens, *styles, hint);
        place(hintLabel, colX[0], 820.0f);

        // T toggles the theme; PaintSystem repaints every painted entity.
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
