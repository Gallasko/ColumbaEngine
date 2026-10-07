#include "progressgallery.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <string>

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"

#include "Core/motion.h"
#include "UI/progressrule.h"
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

        const char* RUNNING_CAPTION = "MONTH 0 OF 6 \xC2\xB7 STRENGTH 14 \xE2\x86\x92 17 AT TERM";
    }

    void ProgressGallery::init()
    {
        theme = ecsRef->getSystem<ThemeSystem>();

        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, PAGE_W, PAGE_H, theme->color("vellum"));
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;
        ecsRef->attach<ThemeComponent>(bg.entity, "scene.background");

        auto place = [](EntityRef e, float x, float y)
        {
            auto p = e->get<PositionComponent>();
            p->setX(x);
            p->setY(y);
        };

        auto caption = [&](float x, float y, const std::string& text, const std::string& color)
        {
            LabelSpec spec; spec.style = "caption"; spec.text = text; spec.color = color; spec.z = 15;
            place(makeLabel(ecsRef, spec).entity, x, y);
        };

        // A static rule with a caption naming it.
        auto row = [&](float x, float y, const std::string& name, const ProgressRuleSpec& spec)
        {
            caption(x, y - 20.0f, name, "ink-muted");
            ProgressRule r = makeProgressRule(ecsRef, spec);
            place(r.root, x, y);
        };

        // ── Left column: static rules ─────────────────────────────────────────
        const float lx = 48.0f;
        row(lx, 60.0f,  "0 %",                 {240.0f, false, 0.0f});
        row(lx, 110.0f, "35 %",                {240.0f, false, 35.0f});
        row(lx, 160.0f, "100 %",               {240.0f, false, 100.0f});
        row(lx, 210.0f, "35 % -> 60 % forecast", {240.0f, false, 35.0f, 60.0f});
        {
            caption(lx, 240.0f, "sm, no nib, 70 %", "ink-muted");
            ProgressRuleSpec sm; sm.width = 240.0f; sm.small = true; sm.percent = 70.0f; sm.nib = false;
            ProgressRule r = makeProgressRule(ecsRef, sm);
            place(r.root, lx, 260.0f);
        }
        {
            caption(lx, 300.0f, "with caption", "ink-muted");
            ProgressRuleSpec cs; cs.width = 240.0f; cs.percent = 50.0f;
            cs.caption = "MONTH 3 OF 6 \xC2\xB7 SWORDSMANSHIP 3 \xE2\x86\x92 4 AT TERM";
            ProgressRule r = makeProgressRule(ecsRef, cs);
            place(r.root, lx, 320.0f);
        }
        {
            caption(lx, 380.0f, "on folio (inside a panel)", "ink-muted");
            Panel panel = makePanel(ecsRef, {PanelFrame::Ruled, 288.0f, "At work now"});
            ProgressRuleSpec ps; ps.width = 256.0f; ps.percent = 35.0f; ps.forecastPercent = 60.0f;
            ps.z = panel.spec.contentZ;
            panel.addChild(ecsRef, makeProgressRule(ecsRef, ps).root);
            place(panel.root, lx, 400.0f);
        }

        // ── Right column: the running activity ────────────────────────────────
        const float rx = 560.0f;
        caption(rx, 60.0f, "the running activity", "ink-muted");
        {
            ProgressRuleSpec rs; rs.width = 480.0f; rs.percent = 0.0f; rs.forecastPercent = 100.0f;
            rs.caption = RUNNING_CAPTION;
            running = makeProgressRule(ecsRef, rs);
            place(running.root, rx, 80.0f);
        }

        struct Btn { const char* label; const char* tag; };
        const Btn btns[3] = {{"Advance a month", "advance"}, {"Reset", "reset"}, {"Set forecast 50", "forecast50"}};
        for (int i = 0; i < 3; ++i)
        {
            Button b = makeButton(ecsRef, {ButtonVariant::Quiet, btns[i].label, "", -1, false, "", btns[i].tag});
            place(b.root, rx + static_cast<float>(i) * 180.0f, 140.0f);
        }

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e)
        {
            if (e.tag == "advance")
            {
                month = std::min(month + 1, 6);
                running.setPercent(ecsRef, static_cast<float>(month) * 100.0f / 6.0f);
                running.setCaption(ecsRef,
                    "MONTH " + std::to_string(month) + " OF 6 \xC2\xB7 STRENGTH 14 \xE2\x86\x92 17 AT TERM");
            }
            else if (e.tag == "reset")
            {
                month = 0;
                running.setPercent(ecsRef, 0.0f, false);
                running.setCaption(ecsRef, RUNNING_CAPTION);
            }
            else if (e.tag == "forecast50")
            {
                running.setForecast(ecsRef, 50.0f);
            }
        });

        // ── Motion + theme controls ───────────────────────────────────────────
        LabelSpec ms; ms.style = "caption"; ms.text = "motion: full"; ms.color = "ink-muted"; ms.z = 15;
        motionLabel = makeLabel(ecsRef, ms);
        place(motionLabel.entity, rx, 200.0f);

        caption(48.0f, 820.0f, "T toggle theme \xC2\xB7 R toggle reduced motion \xC2\xB7 Advance to fill the bar", "ink-muted");

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event)
        {
            if (event.key == SDL_SCANCODE_T)
            {
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
            }
            else if (event.key == SDL_SCANCODE_R)
            {
                Motion::setReduced(not Motion::reduced());
                motionLabel.setText(ecsRef, Motion::reduced() ? "motion: reduced" : "motion: full");
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
