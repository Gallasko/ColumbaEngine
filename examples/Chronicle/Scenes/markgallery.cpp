#include "markgallery.h"

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
#include "UI/sizer.h"
#include "UI/prefab.h"

#include "UI/mark.h"
#include "UI/label.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float CONTENT_Z = 20.0f;
        constexpr float SURFACE_Z = 12.0f;   // below the content band, so painted surfaces sit behind
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;
    }

    void MarkGallery::init()
    {

        // Vellum background (not a layout child; it just fills the page).
        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, PAGE_W, PAGE_H, tokens->colour("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        ecsRef->attach<PaintComponent>(bg.entity, "vellum");

        const float margin = tokens->space(7);            // 48
        const float contentW = PAGE_W - 2.0f * margin;

        // ── helpers ───────────────────────────────────────────────────────────
        auto mark = [this](const std::string& name, MarkSize size)
        {
            return makeMark(ecsRef, *tokens, {name, size, "ink", static_cast<int>(CONTENT_Z)}).entity;
        };

        auto vlayout = [this](float w, size_t spacing)
        {
            auto l = makeVerticalLayout(ecsRef, 0.0f, 0.0f, w, 0.0f);
            l.get<VerticalLayout>()->spacing = spacing;
            return l;
        };

        auto hlayout = [this](float w, size_t spacing, bool wrap = false)
        {
            auto l = makeHorizontalLayout(ecsRef, 0.0f, 0.0f, w, 0.0f);
            l.get<HorizontalLayout>()->spacing = spacing;
            l.get<HorizontalLayout>()->fitToAxis = wrap;
            return l;
        };

        // A surface that tracks a layout's settled bounds, drawn behind its content with a
        // little padding so it reads as a leaf rather than a tight crop. Layout children get
        // their z constrained to the layout's z (== CONTENT_Z), so SURFACE_Z sits behind them.
        auto surfaceBehind = [this](EntityRef layout, const std::string& token, float pad)
        {
            auto s = makeUiSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, tokens->colour(token));
            s.get<PositionComponent>()->setZ(SURFACE_Z);
            auto a = s.get<UiAnchor>();
            a->fillIn(layout->get<UiAnchor>());
            a->setTopMargin(-pad);
            a->setBottomMargin(-pad);
            a->setLeftMargin(-pad);
            a->setRightMargin(-pad);
            ecsRef->attach<PaintComponent>(s.entity, token);
        };

        // A grid cell: mark centred over its name, both centred in a fixed-width box so the
        // wrapping row can space them evenly (layouts left-align, so centring needs anchors).
        auto gridCell = [this](const std::string& name)
        {
            constexpr float CELL_W = 96.0f;
            constexpr float CELL_H = 52.0f;

            auto cell = makeAnchoredPrefab(ecsRef, 0.0f, 0.0f, CONTENT_Z);
            cell.get<PositionComponent>()->setWidth(CELL_W);
            cell.get<PositionComponent>()->setHeight(CELL_H);

            Mark m = makeMark(ecsRef, *tokens, {name, MarkSize::S24, "ink", static_cast<int>(CONTENT_Z)});
            auto ma = m.entity->get<UiAnchor>();
            ma->setTopAnchor(PosAnchor{cell.id, AnchorType::Top});
            ma->setHorizontalCenter(PosAnchor{cell.id, AnchorType::HorizontalCenter});
            ma->setZConstrain(PosConstrain{cell.id, AnchorType::Z, PosOpType::Add, 1.0f});
            cell.get<Prefab>()->addToPrefab(m.entity);

            LabelSpec spec; spec.style = "caption"; spec.text = name; spec.colour = "ink-muted";
            spec.align = Align::Centre; spec.overflow = Overflow::Ellipsis; spec.width = CELL_W;
            spec.z = static_cast<int>(CONTENT_Z);
            Label cap = makeLabel(ecsRef, *tokens, *styles, spec);
            auto ca = cap.entity->get<UiAnchor>();
            ca->setTopAnchor(PosAnchor{cell.id, AnchorType::Top});
            ca->setTopMargin(px(MarkSize::S24) + tokens->space(1));
            ca->setLeftAnchor(PosAnchor{cell.id, AnchorType::Left});
            ca->setZConstrain(PosConstrain{cell.id, AnchorType::Z, PosOpType::Add, 1.0f});
            cell.get<Prefab>()->addToPrefab(cap.entity);

            return cell.entity;
        };

        // ── the page: one vertical column, sections stacked with a region gap ──
        auto page = vlayout(contentW, static_cast<size_t>(tokens->space(6)));   // 32

        page.get<PositionComponent>()->setX(margin);
        page.get<PositionComponent>()->setY(40.0f);
        page.get<PositionComponent>()->setZ(CONTENT_Z);   // children inherit this z (see sizer zConstrain)

        auto* pageLayout = page.get<VerticalLayout>().component;

        // ── Row 1: the whole set at S24 in a wrapping grid, name under each ────
        {
            auto row = hlayout(contentW, static_cast<size_t>(tokens->space(4)), /*wrap*/ true);   // 16

            for (const auto& name : markNames())
                row.get<HorizontalLayout>()->addEntity(gridCell(name));

            pageLayout->addEntity(row.entity);
        }

        // ── Row 2: the set at S14 on a vellum-worn strip (legibility floor) ───
        {
            auto row = hlayout(contentW, static_cast<size_t>(tokens->space(2)));   // 8
            surfaceBehind(row.entity, "vellum-worn", tokens->space(2));

            for (const auto& name : markNames())
                row.get<HorizontalLayout>()->addEntity(mark(name, MarkSize::S14));

            pageLayout->addEntity(row.entity);
        }

        // ── Mid: pairs (left) beside the status check (right) ─────────────────
        {
            auto mid = hlayout(contentW, static_cast<size_t>(tokens->space(6)));   // 32

            // Pairs, one per size, with a hair-rule divider under each.
            auto col = vlayout(380.0f, static_cast<size_t>(tokens->space(2)));

            struct Pair
            {
                const char* mark;
                const char* style;
                const char* text;
                const char* colour;
            };

            const Pair pairs[6] = {
                {"time", "tick", "+3 mo", "ink"},
                {"gold", "body-sm", "412 gold", "ink"},
                {"strength", "body", "Strength", "ink"},
                {"reputation", "figure", "STANDING 3", "ink"},
                {"adventure", "title", "North Forest", "ink"},
                {"seal", "versal", "A", "vermilion"},
            };

            for (const auto& p : pairs)
            {
                MarkedLabelSpec spec;
                spec.mark = p.mark;
                spec.label = {p.style, p.text, p.colour};
                spec.z = static_cast<int>(CONTENT_Z);
                col.get<VerticalLayout>()->addEntity(makeMarkedLabel(ecsRef, *tokens, *styles, spec).root);

                auto rule = makeUiSimple2DShape(ecsRef, Shape2D::Square, 380.0f, 1.0f, tokens->colour("rule-hair"));
                ecsRef->attach<PaintComponent>(rule.entity, "rule-hair");
                col.get<VerticalLayout>()->addEntity(rule.entity);
            }

            mid.get<HorizontalLayout>()->addEntity(col.entity);

            // Status: the mark alone must carry the state, on vellum then on folio.
            struct Status
            {
                const char* mark;
                const char* text;
                const char* colour;
            };

            const Status rows[2] = {
                {"check", "Strength 14 of 12", "status-gain"},
                {"cross", "Coin 40 of 60", "status-loss"},
            };

            auto addStatus = [&](VerticalLayout* into)
            {
                for (const auto& r : rows)
                {
                    MarkedLabelSpec spec;
                    spec.mark = r.mark;
                    spec.label = {"body", r.text, r.colour};
                    spec.z = static_cast<int>(CONTENT_Z);
                    into->addEntity(makeMarkedLabel(ecsRef, *tokens, *styles, spec).root);
                }
            };

            auto statusCol = vlayout(300.0f, static_cast<size_t>(tokens->space(4)));
            addStatus(statusCol.get<VerticalLayout>().component);

            auto folioGroup = vlayout(300.0f, static_cast<size_t>(tokens->space(2)));
            surfaceBehind(folioGroup.entity, "folio", tokens->space(3));
            addStatus(folioGroup.get<VerticalLayout>().component);
            statusCol.get<VerticalLayout>()->addEntity(folioGroup.entity);

            mid.get<HorizontalLayout>()->addEntity(statusCol.entity);
            pageLayout->addEntity(mid.entity);
        }

        // ── Row 5: the missing-mark case draws "seal" and logs once ───────────
        {
            auto row = hlayout(contentW, static_cast<size_t>(tokens->space(2)));
            row.get<HorizontalLayout>()->addEntity(mark("clock", MarkSize::S24));

            LabelSpec spec; spec.style = "caption"; spec.text = "\"clock\" -> seal"; spec.colour = "ink-muted";
            spec.z = static_cast<int>(CONTENT_Z);
            row.get<HorizontalLayout>()->addEntity(makeLabel(ecsRef, *tokens, *styles, spec).entity);
            pageLayout->addEntity(row.entity);
        }

        // Footer hint.
        {
            LabelSpec spec; spec.style = "caption"; spec.text = "T  toggle theme"; spec.colour = "ink-muted";
            spec.z = static_cast<int>(CONTENT_Z);
            pageLayout->addEntity(makeLabel(ecsRef, *tokens, *styles, spec).entity);
        }

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
