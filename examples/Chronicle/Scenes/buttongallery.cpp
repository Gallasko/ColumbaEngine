#include "buttongallery.h"

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

#include "UI/button.h"
#include "UI/panel.h"
#include "UI/label.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;
    }

    void ButtonGallery::init()
    {
        paintSystem = ecsRef->getSystem<PaintSystem>();

        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, PAGE_W, PAGE_H, tokens->colour("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        paintSystem->paint(bg.entity, "vellum");

        // A folio leaf under the buttons (z below the Panels band).
        auto leaf = makeUiSimple2DShape(ecsRef, Shape2D::Square, 1000.0f, 520.0f, tokens->colour("folio"));
        leaf.get<PositionComponent>()->setX(40.0f);
        leaf.get<PositionComponent>()->setY(48.0f);
        leaf.get<PositionComponent>()->setZ(8.0f);
        paintSystem->paint(leaf.entity, "folio");

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

        // A row of buttons laid out left to right.
        auto row = [&](float x, float y, size_t spacing, const std::vector<ButtonSpec>& specs)
        {
            auto layout = makeHorizontalLayout(ecsRef, x, y, 1000.0f, 0.0f);
            layout.get<HorizontalLayout>()->spacing = spacing;
            layout.get<PositionComponent>()->setZ(20.0f);
            for (const auto& spec : specs)
                layout.get<HorizontalLayout>()->addEntity(makeButton(ecsRef, *tokens, *styles, spec).root);
        };

        const float margin = 64.0f;

        // Row 1: the three variants.
        caption(margin, 60.0f, "variants", "ink-muted");
        row(margin, 80.0f, static_cast<size_t>(tokens->space(5)), {
            {ButtonVariant::Quiet, "Train", "", -1, false, "", "train"},
            {ButtonVariant::Study, "Study", "study", -1, false, "", "study"},
            {ButtonVariant::Seal, "Take the path", "seal", -1, false, "", "seal"},
        });

        // Row 2: with a month cost.
        caption(margin, 140.0f, "with cost", "ink-muted");
        row(margin, 160.0f, static_cast<size_t>(tokens->space(5)), {
            {ButtonVariant::Quiet, "Train at the yard", "training", 6, false, "", "train-yard"},
            {ButtonVariant::Study, "Read at the academy", "study", 9, false, "", "read"},
            {ButtonVariant::Seal, "Enter the Ruins", "seal", 3, false, "", "ruins"},
        });

        // Row 3: disabled with a reason (placed by hand, they are taller).
        caption(margin, 220.0f, "disabled + reason", "ink-muted");
        {
            ButtonSpec sq; sq.variant = ButtonVariant::Quiet; sq.label = "Squire"; sq.disabled = true;
            sq.reason = "SWORDSMANSHIP 3 OF 4 \xC2\xB7 THE DOOR CLOSES AT 22"; sq.tag = "squire";
            place(makeButton(ecsRef, *tokens, *styles, sq).root, margin, 240.0f);

            ButtonSpec tk; tk.variant = ButtonVariant::Seal; tk.label = "Take the path"; tk.glyph = "seal";
            tk.disabled = true; tk.reason = "STRENGTH 14 OF 18"; tk.tag = "take-locked";
            place(makeButton(ecsRef, *tokens, *styles, tk).root, 420.0f, 240.0f);
        }

        // Row 4: dense, no glyphs, the tab-order row.
        caption(margin, 320.0f, "dense (tab order)", "ink-muted");
        row(margin, 340.0f, static_cast<size_t>(tokens->space(2)), {
            {ButtonVariant::Quiet, "One", "", -1, false, "", "one"},
            {ButtonVariant::Quiet, "Two", "", -1, false, "", "two"},
            {ButtonVariant::Quiet, "Three", "", -1, false, "", "three"},
            {ButtonVariant::Quiet, "Four", "", -1, false, "", "four"},
            {ButtonVariant::Quiet, "Five", "", -1, false, "", "five"},
        });

        // Row 5: overlap - a button half-covered by a ruled panel at a higher z.
        caption(margin, 420.0f, "overlap (covered half must not tint)", "ink-muted");
        {
            ButtonSpec ov; ov.variant = ButtonVariant::Quiet; ov.label = "Half covered"; ov.tag = "covered";
            place(makeButton(ecsRef, *tokens, *styles, ov).root, margin, 440.0f);

            Panel cover = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Ruled, 160.0f, "Cover", "", "", 30, 40});
            cover.addChild(ecsRef, makeLabel(ecsRef, *tokens, *styles, {"body", "on top", "ink", Align::Left, Overflow::Grow, 0.0f, 0, 40}).box);
            place(cover.root, margin + 90.0f, 430.0f);
        }

        // Footer: instructions + the last activated tag.
        caption(margin, 820.0f, "Tab / Shift-Tab focus \xC2\xB7 Enter or Space activates \xC2\xB7 click a button to log its tag", "ink-muted");

        LabelSpec ts; ts.style = "body-sm"; ts.text = "(no activation yet)"; ts.colour = "ink-muted"; ts.z = 15;
        lastTag = makeLabel(ecsRef, *tokens, *styles, ts);
        place(lastTag.box, 980.0f, 820.0f);

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& event)
        {
            lastTag.setText(ecsRef, *styles, "activated: " + event.tag);
        });

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
