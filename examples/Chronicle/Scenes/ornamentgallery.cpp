#include "ornamentgallery.h"

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
#include "2D/decoratedshapes.h"
#include "UI/sizer.h"

#include "UI/ornament.h"
#include "UI/label.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float CONTENT_Z = 20.0f;
        constexpr float SURFACE_Z = 12.0f;
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;
    }

    void OrnamentGallery::init()
    {
        paintSystem = ecsRef->getSystem<PaintSystem>();

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
            LabelSpec spec; spec.style = "caption"; spec.text = text; spec.colour = colour;
            spec.z = static_cast<int>(CONTENT_Z);
            Label l = makeLabel(ecsRef, *tokens, *styles, spec);
            place(l.box, x, y);
        };

        auto label = [&](const std::string& style, float x, float y, const std::string& text, const std::string& colour)
        {
            LabelSpec spec; spec.style = style; spec.text = text; spec.colour = colour;
            spec.z = static_cast<int>(CONTENT_Z);
            Label l = makeLabel(ecsRef, *tokens, *styles, spec);
            place(l.box, x, y);
            return l;
        };

        // One divider row: a caption naming it, then the divider stretched to the column width.
        struct Div { const char* name; DividerWeight weight; bool knot; };
        auto dividerRow = [&](float x, float y, float w, const Div& d, const std::string& ground) -> float
        {
            caption(x, y, d.name, "ink-muted");
            OrnamentSpec spec;
            spec.kind = OrnamentKind::Divider; spec.weight = d.weight; spec.knot = d.knot;
            spec.width = w; spec.ground = ground; spec.z = static_cast<int>(CONTENT_Z);
            Ornament orn = makeOrnament(ecsRef, *tokens, *styles, spec);
            place(orn.root, x, y + 24.0f);
            return y + 24.0f + tokens->space(3) + 6.0f;
        };

        const Div divs[4] = {
            {"hair + knot", DividerWeight::Hair, true},
            {"hair",        DividerWeight::Hair, false},
            {"rule + knot", DividerWeight::Rule, true},
            {"rule",        DividerWeight::Rule, false},
        };

        // ── Left column, on a folio leaf ──────────────────────────────────────
        const float leftX = 64.0f;
        const float colW = 520.0f;
        {
            auto leaf = makeUiSimple2DShape(ecsRef, Shape2D::Square, colW + 32.0f, 760.0f, tokens->colour("folio"));
            leaf.get<PositionComponent>()->setZ(SURFACE_Z);
            place(leaf.entity, leftX - 16.0f, 32.0f);
            paintSystem->paint(leaf.entity, "folio");

            float y = 56.0f;
            for (const auto& d : divs)
                y = dividerRow(leftX, y, colW, d, "folio");

            // Flourishes under a title header. z = CONTENT_Z so they sit above the folio leaf.
            y += 12.0f;
            label("title", leftX, y, "North Forest", "ink");
            OrnamentSpec f1s; f1s.kind = OrnamentKind::Flourish; f1s.z = static_cast<int>(CONTENT_Z);
            Ornament f1 = makeOrnament(ecsRef, *tokens, *styles, f1s);
            place(f1.root, leftX, y + 40.0f);
            y += 76.0f;

            label("title", leftX, y, "The Drowned Keep", "vermilion");
            OrnamentSpec fs; fs.kind = OrnamentKind::Flourish; fs.colour = "vermilion"; fs.z = static_cast<int>(CONTENT_Z);
            Ornament f2 = makeOrnament(ecsRef, *tokens, *styles, fs);
            place(f2.root, leftX, y + 40.0f);
            y += 96.0f;

            // Versals side by side, each tone.
            const VersalTone tones[3] = {VersalTone::Vermilion, VersalTone::Gold, VersalTone::Lapis};
            const char* letters[3] = {"A", "I", "W"};
            for (int i = 0; i < 3; ++i)
            {
                OrnamentSpec vs; vs.kind = OrnamentKind::Versal; vs.letter = letters[i]; vs.tone = tones[i];
                vs.z = static_cast<int>(CONTENT_Z);
                Ornament v = makeOrnament(ecsRef, *tokens, *styles, vs);
                place(v.root, leftX + static_cast<float>(i) * 92.0f, y);
            }
            y += 92.0f;

            // The milestone spread: a versal beside a chapter line that wraps in the column.
            OrnamentSpec ms; ms.kind = OrnamentKind::Versal; ms.letter = "I"; ms.tone = VersalTone::Vermilion;
            ms.z = static_cast<int>(CONTENT_Z);
            Ornament milestone = makeOrnament(ecsRef, *tokens, *styles, ms);
            place(milestone.root, leftX, y);

            LabelSpec msl;
            msl.style = "chapter"; msl.text = "n his fourteenth year, Aldren chose his path."; msl.colour = "ink";
            msl.overflow = Overflow::Wrap; msl.width = colW - 88.0f; msl.z = static_cast<int>(CONTENT_Z);
            Label msLabel = makeLabel(ecsRef, *tokens, *styles, msl);
            place(msLabel.box, leftX + 88.0f, y + 4.0f);
        }

        // ── Right column, on bare vellum ──────────────────────────────────────
        const float rightX = 700.0f;
        {
            float y = 56.0f;
            for (const auto& d : divs)
                y = dividerRow(rightX, y, colW, d, "vellum");

            // The illuminated frame: a doubled gold stroke with the four corners 2 px inside.
            y += 12.0f;
            caption(rightX, y, "illuminated frame", "ink-muted");
            y += 24.0f;
            const float fw = 320.0f, fh = 160.0f;
            auto frame = makeStrokeRect2DShape(ecsRef, fw, fh, tokens->colour("gold-edge"), 1.0f, 1.0f, true);
            frame.get<PositionComponent>()->setZ(CONTENT_Z);
            place(frame.entity, rightX, y);
            paintSystem->paint(frame.entity, "gold-edge");

            const CornerPos corners[4] = {CornerPos::TL, CornerPos::TR, CornerPos::BL, CornerPos::BR};
            const float cx[4] = {rightX + 2.0f, rightX + fw - 28.0f - 2.0f, rightX + 2.0f, rightX + fw - 28.0f - 2.0f};
            const float cy[4] = {y + 2.0f, y + 2.0f, y + fh - 28.0f - 2.0f, y + fh - 28.0f - 2.0f};
            for (int i = 0; i < 4; ++i)
            {
                OrnamentSpec cs; cs.kind = OrnamentKind::Corner; cs.corner = corners[i];
                cs.z = static_cast<int>(CONTENT_Z);
                Ornament c = makeOrnament(ecsRef, *tokens, *styles, cs);
                place(c.root, cx[i], cy[i]);
            }
            y += fh + 24.0f;

            // The ruled list: body rows separated by knotless hair dividers, in a layout.
            caption(rightX, y, "ruled list", "ink-muted");
            y += 24.0f;
            auto list = makeVerticalLayout(ecsRef, rightX, y, colW, 0.0f);
            list.get<VerticalLayout>()->spacing = static_cast<size_t>(tokens->space(3));
            list.get<PositionComponent>()->setZ(CONTENT_Z);
            const char* rows[5] = {"Strength", "Dexterity", "Intelligence", "Vitality", "Reputation"};
            for (int i = 0; i < 5; ++i)
            {
                LabelSpec rs; rs.style = "body"; rs.text = rows[i]; rs.colour = "ink"; rs.z = static_cast<int>(CONTENT_Z);
                list.get<VerticalLayout>()->addEntity(makeLabel(ecsRef, *tokens, *styles, rs).box);

                if (i < 4)
                {
                    OrnamentSpec ds;
                    ds.kind = OrnamentKind::Divider; ds.weight = DividerWeight::Hair; ds.knot = false;
                    ds.width = colW; ds.ground = "vellum"; ds.z = static_cast<int>(CONTENT_Z);
                    list.get<VerticalLayout>()->addEntity(makeOrnament(ecsRef, *tokens, *styles, ds).root);
                }
            }
        }

        caption(leftX, 820.0f, "T  toggle theme", "ink-muted");

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
