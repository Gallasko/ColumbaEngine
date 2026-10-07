#include "requirementgallery.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
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
#include "Systems/gamefacts.h"

#include "UI/requirementlist.h"
#include "UI/panel.h"
#include "UI/button.h"
#include "UI/label.h"

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
        theme = ecsRef->getSystem<ThemeSystem>();

        auto facts = ecsRef->getSystem<WorldFacts>();

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
            LabelSpec spec;
            spec.style = "caption";
            spec.text = text;
            spec.color = color;
            spec.z = 15;

            place(makeLabel(ecsRef, spec).entity, x, y);
        };

        const float margin = theme->space(7);   // 48

        const std::vector<Requirement> warrior = {
            {"Strength", 15, 18},
            {"Swordsmanship", 3, 4},
            {"Vitality", 12, 10},
            {"Has the Guild's letter", -1, 0, 1},
        };

        // ── Left: the milestone panel, roomy ─────────────────────────────────
        {
            Panel panel = makePanel(ecsRef, {PanelFrame::Illuminated, 360.0f, "Warrior", "swordsmanship", "AT 18"});

            RequirementListSpec ls;
            ls.width = panel.innerWidth();
            ls.items = warrior;
            ls.z = panel.spec.contentZ;
            panel.addChild(ecsRef, makeRequirementList(ecsRef, ls).root);
            place(panel.root, margin, 90.0f);
        }

        // ── Under it: the same list, dense, on bare vellum ───────────────────
        {
            caption(margin, 388.0f, "dense (inside a row)", "ink-muted");
            RequirementListSpec ds;
            ds.width = 256.0f;
            ds.dense = true;
            ds.items = warrior;
            place(makeRequirementList(ecsRef, ds).root, margin, 408.0f);
        }

        // Right: the Squire panel, fed from the facts
        const float rx = 520.0f;
        {
            Panel panel = makePanel(ecsRef, {PanelFrame::Ruled, 320.0f, "Squire", "training", "PATH"});

            RequirementListSpec fs;
            fs.width = panel.innerWidth();
            fs.items = {
                {SQUIRE[0].label, SQUIRE[0].start, SQUIRE[0].needed},
                {SQUIRE[1].label, SQUIRE[1].start, SQUIRE[1].needed},
                {SQUIRE[2].label, SQUIRE[2].start, SQUIRE[2].needed},
                {"Has the Guild's letter", -1, 0, 0},
            };
            fs.z = panel.spec.contentZ;
            fed = makeRequirementList(ecsRef, fs);
            panel.addChild(ecsRef, fed.root);
            place(panel.root, rx, 90.0f);
        }
        caption(rx, 68.0f, "fed from WorldFacts \xC2\xB7 buttons write facts, the list reacts", "ink-muted");

        // Seed the facts, then listen for the rows: buttons write, the list reacts.
        if (facts)
        {
            for (size_t i = 0; i < 3; ++i)
                facts->setFact(curPath(i), SQUIRE[i].start);

            facts->setFact(metPath(3), false);
        }

        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            for (const auto& name : event.changedFacts)
            {
                const auto& it = event.factMap->find(name);

                if (it == event.factMap->end())
                    continue;

                for (size_t i = 0; i < 3; ++i)
                {
                    if (name == curPath(i))
                        fed.setItem(ecsRef, i, it->second.get<int>(), SQUIRE[i].needed);
                }

                if (name == metPath(3))
                    fed.setMet(ecsRef, 3, it->second.get<bool>());
            }
        });

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
            Button b = makeButton(ecsRef, {ButtonVariant::Quiet, btns[i].label, "", -1, false, "", btns[i].tag});
            place(b.root, bx, by + static_cast<float>(i) * 44.0f);
        }

        LabelSpec es; es.style = "body-sm"; es.text = "ready"; es.color = "ink-muted"; es.z = 15;
        echo = makeLabel(ecsRef, es);
        place(echo.entity, bx, by + 5.0f * 44.0f + 12.0f);

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e)
        {
            auto facts = ecsRef->getSystem<WorldFacts>();

            if (not facts)
                return;

            if (e.tag == "str")
            {
                const int n = facts->getFact<int>(curPath(0)) + 1;
                facts->setFact(curPath(0), n);
                echo.setText(ecsRef, "Strength " + std::to_string(n) + " / " + std::to_string(SQUIRE[0].needed));
            }
            else if (e.tag == "sword")
            {
                const int n = facts->getFact<int>(curPath(1)) + 1;
                facts->setFact(curPath(1), n);
                echo.setText(ecsRef, "Swordsmanship " + std::to_string(n) + " / " + std::to_string(SQUIRE[1].needed));
            }
            else if (e.tag == "grant")
            {
                facts->setFact(metPath(3), true);
                echo.setText(ecsRef, "The letter is granted");
            }
            else if (e.tag == "revoke")
            {
                facts->setFact(metPath(3), false);
                echo.setText(ecsRef, "The letter is revoked");
            }
            else if (e.tag == "reset")
            {
                for (size_t i = 0; i < 3; ++i)
                    facts->setFact(curPath(i), SQUIRE[i].start);

                facts->setFact(metPath(3), false);
                echo.setText(ecsRef, "Reset");
            }
        });

        // ── Bottom: the ellipsis and the shared right edge ───────────────────
        {
            caption(margin, 608.0f, "an over-long label leaves room for the pair", "ink-muted");
            RequirementListSpec os;
            os.width = 256.0f;
            os.items = {{"Reputation among the Bellmoor carters and the eastern road guild", 40, 22}};
            place(makeRequirementList(ecsRef, os).root, margin, 628.0f);

            caption(margin, 680.0f, "pairs right-align: the slashes stack", "ink-muted");
            RequirementListSpec ps;
            ps.width = 256.0f;
            ps.items = {
                {"Caravan miles", 100, 120},
                {"Letters carried", 9, 18},
            };
            place(makeRequirementList(ecsRef, ps).root, margin, 700.0f);
        }

        caption(margin, 820.0f, "T toggle theme \xC2\xB7 the mark carries the state, the label carries the words", "ink-muted");

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event)
        {
            if (event.key == SDL_SCANCODE_T)
            {
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
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
