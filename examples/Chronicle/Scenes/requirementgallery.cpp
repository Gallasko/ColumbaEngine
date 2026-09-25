#include "requirementgallery.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <string>

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"

#include "UI/requirementlist.h"
#include "UI/panel.h"
#include "UI/button.h"
#include "UI/label.h"
#include "UI/gamedataview.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;

        // The Squire path: three numeric rows and the letter. The scene owns the numbers;
        // the list only draws them.
        struct SquireReq { const char* label; int start; int needed; };
        const SquireReq SQUIRE[3] = {
            {"Strength", 12, 18},
            {"Swordsmanship", 2, 4},
            {"Vitality", 9, 8},
        };

        std::string curPath(size_t i) { return "milestone.squire.reqs." + std::to_string(i) + ".current"; }
        std::string metPath(size_t i) { return "milestone.squire.reqs." + std::to_string(i) + ".met"; }
    }

    void RequirementGallery::init()
    {
        auto* view = ecsRef->getSystem<GameDataView>();

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

        auto caption = [&](float x, float y, const std::string& text, const std::string& colour)
        {
            LabelSpec spec;
            spec.style = "caption";
            spec.text = text;
            spec.colour = colour;
            spec.z = 15;

            place(makeLabel(ecsRef, *tokens, *styles, spec).entity, x, y);
        };

        const float margin = tokens->space(7);   // 48

        const std::vector<Requirement> warrior = {
            {"Strength", 15, 18},
            {"Swordsmanship", 3, 4},
            {"Vitality", 12, 10},
            {"Has the Guild's letter", -1, 0, 1},
        };

        // ── Left: the milestone panel, roomy ─────────────────────────────────
        {
            Panel panel = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Illuminated, 360.0f, "Warrior", "swordsmanship", "AT 18"});

            RequirementListSpec ls;
            ls.width = panel.innerWidth();
            ls.items = warrior;
            ls.z = panel.spec.contentZ;
            panel.addChild(ecsRef, makeRequirementList(ecsRef, *tokens, *styles, ls).root);
            place(panel.root, margin, 90.0f);
        }

        // ── Under it: the same list, dense, on bare vellum ───────────────────
        {
            caption(margin, 388.0f, "dense (inside a row)", "ink-muted");
            RequirementListSpec ds;
            ds.width = 256.0f;
            ds.dense = true;
            ds.items = warrior;
            place(makeRequirementList(ecsRef, *tokens, *styles, ds).root, margin, 408.0f);
        }

        // ── Right: the Squire panel, fed from GameDataView ───────────────────
        const float rx = 520.0f;
        {
            Panel panel = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Ruled, 320.0f, "Squire", "training", "PATH"});

            RequirementListSpec fs;
            fs.width = panel.innerWidth();
            fs.items = {
                {SQUIRE[0].label, SQUIRE[0].start, SQUIRE[0].needed},
                {SQUIRE[1].label, SQUIRE[1].start, SQUIRE[1].needed},
                {SQUIRE[2].label, SQUIRE[2].start, SQUIRE[2].needed},
                {"Has the Guild's letter", -1, 0, 0},
            };
            fs.z = panel.spec.contentZ;
            fed = makeRequirementList(ecsRef, *tokens, *styles, fs);
            panel.addChild(ecsRef, fed.root);
            place(panel.root, rx, 90.0f);
        }
        caption(rx, 68.0f, "fed from GameDataView \xC2\xB7 buttons write paths, the list reacts", "ink-muted");

        // Seed the view, then subscribe the rows: buttons write, the list reacts.
        if (view)
        {
            for (size_t i = 0; i < 3; ++i)
                view->set(curPath(i), ElementType{SQUIRE[i].start});
            view->set(metPath(3), ElementType{false});

            for (size_t i = 0; i < 3; ++i)
            {
                const int needed = SQUIRE[i].needed;
                view->subscribe(curPath(i), [this, i, needed](const ElementType& v) {
                    fed.setItem(ecsRef, *styles, i, v.get<int>(), needed);
                });
            }
            view->subscribe(metPath(3), [this](const ElementType& v) {
                fed.setMet(ecsRef, 3, v.get<bool>());
            });
        }

        // ── The buttons ──────────────────────────────────────────────────────
        struct Btn { const char* label; const char* tag; };
        const Btn btns[5] = {
            {"+1 Strength", "str"},
            {"+1 Swordsmanship", "sword"},
            {"Grant the letter", "grant"},
            {"Revoke the letter", "revoke"},
            {"Reset", "reset"},
        };
        const float bx = 880.0f;
        const float by = 90.0f;
        for (int i = 0; i < 5; ++i)
        {
            Button b = makeButton(ecsRef, *tokens, *styles, {ButtonVariant::Quiet, btns[i].label, "", -1, false, "", btns[i].tag});
            place(b.root, bx, by + static_cast<float>(i) * 44.0f);
        }

        LabelSpec es; es.style = "body-sm"; es.text = "ready"; es.colour = "ink-muted"; es.z = 15;
        echo = makeLabel(ecsRef, *tokens, *styles, es);
        place(echo.entity, bx, by + 5.0f * 44.0f + 12.0f);

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e)
        {
            auto* v = ecsRef->getSystem<GameDataView>();
            if (not v)
                return;

            if (e.tag == "str")
            {
                const int n = v->get(curPath(0)).get<int>() + 1;
                v->set(curPath(0), ElementType{n});
                echo.setText(ecsRef, "Strength " + std::to_string(n) + " / " + std::to_string(SQUIRE[0].needed));
            }
            else if (e.tag == "sword")
            {
                const int n = v->get(curPath(1)).get<int>() + 1;
                v->set(curPath(1), ElementType{n});
                echo.setText(ecsRef, "Swordsmanship " + std::to_string(n) + " / " + std::to_string(SQUIRE[1].needed));
            }
            else if (e.tag == "grant")
            {
                v->set(metPath(3), ElementType{true});
                echo.setText(ecsRef, "The letter is granted");
            }
            else if (e.tag == "revoke")
            {
                v->set(metPath(3), ElementType{false});
                echo.setText(ecsRef, "The letter is revoked");
            }
            else if (e.tag == "reset")
            {
                for (size_t i = 0; i < 3; ++i)
                    v->set(curPath(i), ElementType{SQUIRE[i].start});
                v->set(metPath(3), ElementType{false});
                echo.setText(ecsRef, "Reset");
            }
        });

        // ── Bottom: the ellipsis and the shared right edge ───────────────────
        {
            caption(margin, 608.0f, "an over-long label leaves room for the pair", "ink-muted");
            RequirementListSpec os;
            os.width = 256.0f;
            os.items = {{"Reputation among the Bellmoor carters and the eastern road guild", 40, 22}};
            place(makeRequirementList(ecsRef, *tokens, *styles, os).root, margin, 628.0f);

            caption(margin, 680.0f, "pairs right-align: the slashes stack", "ink-muted");
            RequirementListSpec ps;
            ps.width = 256.0f;
            ps.items = {
                {"Caravan miles", 100, 120},
                {"Letters carried", 9, 18},
            };
            place(makeRequirementList(ecsRef, *tokens, *styles, ps).root, margin, 700.0f);
        }

        caption(margin, 820.0f, "T toggle theme \xC2\xB7 the mark carries the state, the label carries the words", "ink-muted");

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
