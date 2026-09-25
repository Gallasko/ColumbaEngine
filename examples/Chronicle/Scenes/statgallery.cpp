#include "statgallery.h"

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

#include "Core/motion.h"
#include "UI/statline.h"
#include "UI/panel.h"
#include "UI/button.h"
#include "UI/gloss.h"
#include "UI/label.h"
#include "UI/gamedataview.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr float PAGE_W = 1320.0f;
        constexpr float PAGE_H = 860.0f;

        const char* ARROW = "\xE2\x86\x92";   // U+2192
        const char* DASH  = "\xE2\x80\x94";   // U+2014 em dash, for "none"

        struct Part
        {
            const char* id;        // "str", "dex", "int", "vit"
            const char* label;
            const char* glyph;
            int value;
            int projected;         // < 0 = none
            int threshold;         // 0 = none
            const char* note;      // "" = none
            const char* glossText;
        };

        const Part PARTS[4] = {
            {"str", "Strength", "strength",     14, 17, 18, "WARRIOR AT 18 ASKS 18",
             "Lifting, striking, enduring. Grows at the yard and in the mines."},
            {"dex", "Dexterity", "dexterity",    9, -1, 12, "SCOUT AT 16 ASKS 12",
             "Speed of hand and foot. The difference between a parry and a wound."},
            {"int", "Intellect", "intelligence", 11, 12,  0, "",
             "Reading the world and the book. Grows at study and in the academy."},
            {"vit", "Vitality", "vitality",     16, -1,  0, "",
             "Wind and blood. What a long road spends and a long rest returns."},
        };

        std::string path(const char* id)            { return std::string("character.parts.") + id; }
        std::string projPath(const char* id)        { return path(id) + ".projected"; }
        std::string thrPath(const char* id)         { return path(id) + ".threshold"; }
    }

    void StatGallery::init()
    {
        auto* view = ecsRef->getSystem<GameDataView>();
        auto* reg  = ecsRef->getSystem<GlossRegistry>();

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

        // -- Register each part's gloss (Now / At term / Next milestone asks) --
        if (reg)
        {
            for (const auto& p : PARTS)
            {
                GlossSpec g;
                g.title = p.label;
                g.text = p.glossText;
                g.rows = {
                    {"Now", std::to_string(p.value)},
                    {"At term", p.projected > p.value ? std::to_string(p.projected) : DASH},
                    {"Next milestone asks", p.threshold > 0 ? std::to_string(p.threshold) : DASH},
                };

                if (p.note[0] != '\0')
                    g.footnote = p.note;

                reg->set(std::string("parts/") + p.id, g);
            }
        }

        // ── Left: the Parts panel, four lines fed from the view ──────────────
        Panel panel = makePanel(ecsRef, *tokens, *styles, {PanelFrame::Ruled, 320.0f, "Parts", "strength", "LEDGER"});
        for (int i = 0; i < 4; ++i)
        {
            const Part& p = PARTS[i];

            StatLineSpec ls;
            ls.width = 288.0f;
            ls.label = p.label;
            ls.glyph = p.glyph;
            ls.value = p.value;
            ls.projected = p.projected;
            ls.threshold = p.threshold;
            ls.note = p.note;
            ls.glossKey = std::string("parts/") + p.id;
            ls.z = panel.spec.contentZ;

            parts[static_cast<size_t>(i)] = makeStatLine(ecsRef, *tokens, *styles, ls);
            panel.addChild(ecsRef, parts[static_cast<size_t>(i)].root);

            // Seed the view (stores only; subscriptions are wired below).
            if (view)
            {
                view->set(path(p.id), ElementType{p.value});

                if (p.projected >= 0)
                    view->set(projPath(p.id), ElementType{p.projected});

                view->set(thrPath(p.id), ElementType{p.threshold});
            }
        }
        place(panel.root, margin, 120.0f);
        caption(margin, 98.0f, "fed from GameDataView \xC2\xB7 hover a line for its gloss (200 ms)", "ink-muted");

        // ── The lines react through subscriptions; nothing calls a setter directly ──
        if (view)
        {
            for (int i = 0; i < 4; ++i)
            {
                const char* id = PARTS[i].id;
                const size_t idx = static_cast<size_t>(i);
                view->subscribe(path(id), [this, idx](const ElementType& v) {
                    parts[idx].setValue(ecsRef, *styles, v.get<int>());
                });
                view->subscribe(projPath(id), [this, idx](const ElementType& v) {
                    parts[idx].setProjected(ecsRef, *styles, v.get<int>());
                });
                view->subscribe(thrPath(id), [this, idx](const ElementType& v) {
                    parts[idx].setThreshold(ecsRef, v.get<int>());
                });
            }
        }

        // ── Right: five Quiet buttons that write the view ────────────────────
        struct Btn { const char* label; const char* tag; };
        const Btn btns[5] = {
            {"Train: STR +1", "train_str"},
            {"Study: INT +1", "study_int"},
            {"Project STR 20", "project_str"},
            {"Clear projections", "clear_proj"},
            {"Milestone passed: STR threshold \xE2\x86\x92 24", "milestone_str"},   // U+2192
        };
        const float bx = 480.0f;
        const float by = 120.0f;
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

            if (e.tag == "train_str")
            {
                const int n = v->get(path("str")).get<int>() + 1;
                v->set(path("str"), ElementType{n});
                echo.setText(ecsRef, std::string("Train: STR ") + ARROW + " " + std::to_string(n));
            }
            else if (e.tag == "study_int")
            {
                const int n = v->get(path("int")).get<int>() + 1;
                v->set(path("int"), ElementType{n});
                echo.setText(ecsRef, std::string("Study: INT ") + ARROW + " " + std::to_string(n));
            }
            else if (e.tag == "project_str")
            {
                v->set(projPath("str"), ElementType{20});
                echo.setText(ecsRef, std::string("Project STR ") + ARROW + " 20");
            }
            else if (e.tag == "clear_proj")
            {
                for (const auto& p : PARTS)
                    v->set(projPath(p.id), ElementType{-1});
                echo.setText(ecsRef, "Cleared projections");
            }
            else if (e.tag == "milestone_str")
            {
                v->set(thrPath("str"), ElementType{24});
                echo.setText(ecsRef, std::string("STR milestone ") + ARROW + " 24");
            }
        });

        // ── Below the panel: one wide line on bare vellum, everything on ──────
        {
            caption(margin, 438.0f, "480 wide on bare vellum \xC2\xB7 the tick stands on the hatch", "ink-muted");
            StatLineSpec bs;
            bs.width = 480.0f;
            bs.label = "Strength";
            bs.glyph = "strength";
            bs.value = 22;
            bs.max = 30;
            bs.projected = 27;
            bs.threshold = 25;
            bs.note = "WARRIOR AT 24 ASKS 24";
            big = makeStatLine(ecsRef, *tokens, *styles, bs);
            place(big.root, margin, 460.0f);
        }

        // ── Motion + theme controls ──────────────────────────────────────────
        LabelSpec ms; ms.style = "caption"; ms.text = "motion: full"; ms.colour = "ink-muted"; ms.z = 15;
        motionLabel = makeLabel(ecsRef, *tokens, *styles, ms);
        place(motionLabel.entity, bx, by + 5.0f * 44.0f + 32.0f);

        caption(margin, 820.0f,
            "T toggle theme \xC2\xB7 R toggle reduced motion \xC2\xB7 buttons write paths, lines react",
            "ink-muted");

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event)
        {
            if (event.key == SDL_SCANCODE_T)
            {
                const Theme next = tokens->theme() == Theme::Day ? Theme::Candle : Theme::Day;
                tokens->setTheme(next);
                ecsRef->sendEvent(ThemeChangedEvent{next});
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
