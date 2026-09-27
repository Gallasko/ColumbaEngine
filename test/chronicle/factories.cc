#include "stdafx.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "UI/factories.h"
#include "UI/panel.h"
#include "UI/label.h"
#include "UI/mark.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/progressrule.h"
#include "UI/statline.h"
#include "UI/requirementlist.h"
#include "UI/lifeclock.h"
#include "UI/gloss.h"
#include "Core/motion.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "UI/prefabbuilder.h"
#include "UI/prefabfactory.h"
#include "UI/prefabloader.h"
#include "UI/enginefactories.h"
#include "UI/tooltip.h"
#include "UI/gamedataview.h"
#include "Systems/tween.h"
#include "Systems/coresystems.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"

#include "mocklogger.h"

using namespace chronicle;

// The Chronicle kinds as prefab factories: props -> XSpec -> makeX, handles by name, and
// the panel's inheritance (inner width, content band) and slot (body layout).
namespace pg
{
    namespace test
    {
        namespace
        {
            struct FactoriesFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                ThemeSystem* theme = nullptr;

                constant::Vector4D color(const std::string& token, const std::string& id = "") { return theme->theme().color(token, id.empty() ? theme->currentTheme() : id); }
                IconSystem* icons = nullptr;
                PrefabFactoryRegistry* registry = nullptr;

                FactoriesFixture()
                {
                    Motion::setReduced(true);
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.createSystem<PrefabSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, PrefabSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    auto* tip = ecs.createSystem<TooltipSystem>();
                    ecs.succeed<MouseHoverSystem, TooltipSystem>();
                    ecs.createSystem<TweenSystem>();
                    ecs.createSystem<GameDataView>();
                    theme = ecs.createSystem<ThemeSystem>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                    tip->setDefaultFont("body-sm");
                    ecs.createSystem<GlossRegistry>();
                    installIconEntries();

                    registry = ecs.createSystem<PrefabFactoryRegistry>();
                    registerEnginePrefabFactories(registry);
                    registerChronicleFactories(registry);

                    ecs.sendEvent(ResizeEvent{1320.0f, 860.0f});
                }

                void installIconEntries()
                {
                    std::vector<IconEntry> marks;
                    const int sizes[5] = {14, 16, 18, 24, 48};
                    for (const auto& name : markNames())
                        for (int s : sizes)
                            marks.push_back({name, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    icons->registerEntriesForTest("chronicle", marks, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle", OpenGLTexture{});

                    std::vector<IconEntry> orn;
                    const char* onames[7] = {"knot", "flourish", "corner-tl", "corner-tr", "corner-bl", "corner-br", "versal-curls"};
                    const int osizes[4] = {22, 28, 72, 120};
                    for (const char* n : onames)
                        for (int os : osizes)
                            orn.push_back({n, os, {os, os}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    icons->registerEntriesForTest("chronicle-ornaments", orn, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle-ornaments", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }
            };

            NodeSpec node(const std::string& kind, const std::string& name, ElementMap props = {})
            {
                NodeSpec n;
                n.kind = kind;
                n.name = name;
                n.props = std::move(props);
                return n;
            }
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, every_kind_is_registered)
        {
            FactoriesFixture f;

            for (const auto& kind : chronicleKinds())
                EXPECT_TRUE(f.registry->hasFactory(kind)) << kind;

            EXPECT_EQ(chronicleKinds().size(), 12u);
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, panel_hands_inner_width_and_content_band_to_its_children)
        {
            FactoriesFixture f;

            NodeSpec spec = node("Panel", "p", {{"frame", "ruled"}, {"width", 300.0f}, {"heading", "Parts"}, {"glyph", "strength"}});
            spec.children.push_back(node("Label", "l", {{"text", "A line that wraps"}, {"overflow", "wrap"}}));

            NodeSpec list = node("RequirementList", "r");
            list.records["items"] = {{{"label", "Strength"}, {"current", 12}, {"needed", 18}}};
            spec.children.push_back(list);

            EntityRef root = buildTree(&f.ecs, spec);

            // Read before the first frame: the sizes and bands are what the factories set.
            auto prefab = root->get<Prefab>();
            EntityRef p = prefab->getEntity("p");
            EntityRef l = prefab->getEntity("l");
            EntityRef r = prefab->getEntity("r");
            ASSERT_FALSE(p.empty());
            ASSERT_FALSE(l.empty());
            ASSERT_FALSE(r.empty());

            // A ruled panel pads by space-4 (16): the inner width is 268, the content band z 20.
            EXPECT_FLOAT_EQ(p->get<PositionComponent>()->z, 10.0f);
            EXPECT_FLOAT_EQ(l->get<PositionComponent>()->width, 268.0f);
            EXPECT_FLOAT_EQ(l->get<PositionComponent>()->z, 20.0f);
            EXPECT_FLOAT_EQ(r->get<PositionComponent>()->width, 268.0f);
            EXPECT_FLOAT_EQ(r->get<PositionComponent>()->z, 20.0f);

            f.settle();

            // Both went into the body layout, as Panel::addChild would have put them (addEntity
            // is event-driven, so the layout holds them after a frame).
            EntityRef body = p->get<Prefab>()->getEntity("body");
            ASSERT_FALSE(body.empty());
            EXPECT_EQ(body->get<VerticalLayout>()->entities.size(), 2u);

            // The list's setters are helpers on its prefab: meeting the requirement flips the mark.
            auto fed = r->get<Prefab>();
            ASSERT_TRUE(fed->hasHelper("setItem"));
            EXPECT_EQ(fed->callHelper<size_t>("size"), 1u);

            fed->callHelper("setItem", size_t{0}, 18, 18);
            EXPECT_EQ(fed->getEntity("row0")->get<Prefab>()->getEntity("mark")->get<IconComponent>()->iconName, "check");
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, nested_panel_moves_to_the_next_band_by_itself)
        {
            FactoriesFixture f;

            NodeSpec outer = node("Panel", "outer", {{"width", 260.0f}, {"heading", "Who he is"}});
            NodeSpec inner = node("Panel", "inner", {{"heading", "Skills"}});
            inner.children.push_back(node("Label", "deep", {{"text", "Swordsmanship"}}));
            outer.children.push_back(inner);

            EntityRef root = buildTree(&f.ecs, outer);

            // Names are scoped: the inner panel is named on the outer's prefab, the label on the inner's.
            EntityRef o = root->get<Prefab>()->getEntity("outer");
            EntityRef i = root->get<Prefab>()->getEntity("inner");
            ASSERT_FALSE(o.empty());
            ASSERT_FALSE(i.empty());
            EntityRef d = i->get<Prefab>()->getEntity("deep");
            ASSERT_FALSE(d.empty());

            // And findEntity walks the tree from the root.
            EXPECT_EQ(root->get<Prefab>()->findEntity("deep").id, d.id);
            EXPECT_TRUE(root->get<Prefab>()->findEntity("ghost").empty());

            EXPECT_FLOAT_EQ(o->get<PositionComponent>()->z, 10.0f);
            EXPECT_FLOAT_EQ(i->get<PositionComponent>()->z, 20.0f);       // the outer content band
            EXPECT_FLOAT_EQ(i->get<PositionComponent>()->width, 228.0f);  // the outer inner width
            EXPECT_FLOAT_EQ(d->get<PositionComponent>()->z, 30.0f);       // the inner content band
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, enum_strings_colors_and_spacing_tokens)
        {
            FactoriesFixture f;

            NodeSpec page = node("", "page");
            page.children.push_back(node("Panel", "lit", {{"frame", "illuminated"}, {"width", 200.0f}}));
            page.children.push_back(node("Label", "ell", {{"text", "x"}, {"overflow", "ellipsis"}, {"align", "right"}, {"width", 100.0f}}));
            page.children.push_back(node("Label", "bad", {{"text", "x"}, {"color", "not-a-token"}}));
            page.children.push_back(node("Button", "seal", {{"label", "Go"}, {"variant", "seal"}, {"tag", "t"}}));
            page.children.push_back(node("ProgressRule", "rule", {{"width", "space-7"}}));
            page.children.push_back(node("Mark", "mark", {{"name", "gold"}, {"size", 24}}));

            EntityRef root = buildTree(&f.ecs, page);
            auto prefab = root->get<Prefab>();

            // An illuminated panel: ground, frame, four corners and the body
            EntityRef lit = prefab->getEntity("lit");
            ASSERT_FALSE(lit.empty());
            EXPECT_EQ(lit->get<Prefab>()->childrenIds.size(), 7u);

            EntityRef ell = prefab->getEntity("ell");
            ASSERT_FALSE(ell.empty());
            EXPECT_EQ(ell->get<TTFText>()->overflow, Overflow::Ellipsis);
            EXPECT_EQ(ell->get<TTFText>()->align, Align::Right);

            EntityRef bad = prefab->getEntity("bad");
            ASSERT_FALSE(bad.empty());
            EXPECT_EQ(bad->get<ThemeComponent>()->element, labelElement("body", "ink"));   // unknown token -> default

            EntityRef seal = prefab->getEntity("seal");
            ASSERT_FALSE(seal.empty());
            auto face = seal->get<Prefab>()->getEntity("face");
            ASSERT_FALSE(face.empty());
            EXPECT_EQ(face->get<ButtonState>()->variant, ButtonVariant::Seal);
            EXPECT_EQ(face->get<ButtonState>()->tag, "t");

            EntityRef rule = prefab->getEntity("rule");
            ASSERT_FALSE(rule.empty());
            EXPECT_FLOAT_EQ(rule->get<PositionComponent>()->width, 48.0f);   // space-7

            EntityRef mark = prefab->getEntity("mark");
            ASSERT_FALSE(mark.empty());
            EXPECT_FLOAT_EQ(mark->get<PositionComponent>()->width, 24.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, records_build_tabs_and_requirement_rows)
        {
            FactoriesFixture f;

            NodeSpec page = node("", "page");

            NodeSpec tabs = node("Tabs", "tabs", {{"tag", "nav"}});
            tabs.records["items"] = {
                {{"label", "Skills"}, {"glyph", "study"}},
                {{"label", "Parts"}, {"glyph", "strength"}, {"badge", 2}},
                {{"label", "Relics"}, {"glyph", "relic"}},
            };
            page.children.push_back(tabs);

            NodeSpec list = node("RequirementList", "reqs");
            list.records["items"] = {
                {{"label", "Strength"}, {"current", 12}, {"needed", 18}},
                {{"label", "Letter"}, {"met", false}},
                {{"label", "Seal"}, {"met", 1}},
            };
            page.children.push_back(list);

            NodeSpec ml = node("MarkedLabel", "ml", {{"mark", "gold"}});
            ml.records["label"] = {{{"text", "412"}, {"style", "figure"}}};
            page.children.push_back(ml);

            EntityRef root = buildTree(&f.ecs, page);
            auto prefab = root->get<Prefab>();

            EntityRef t = prefab->getEntity("tabs");
            ASSERT_FALSE(t.empty());
            auto tabsPrefab = t->get<Prefab>();
            EXPECT_EQ(tabsPrefab->namedChildrenIds.count("tab2"), 1u);
            EXPECT_EQ(tabsPrefab->namedChildrenIds.count("tab3"), 0u);
            EXPECT_EQ(tabsPrefab->getEntity("badge1")->get<TTFText>()->text, "2");
            EXPECT_EQ(tabsPrefab->getEntity("tab0")->get<TabState>()->tag, "nav");

            EntityRef r = prefab->getEntity("reqs");
            ASSERT_FALSE(r.empty());
            auto reqs = r->get<Prefab>();
            EXPECT_EQ(reqs->callHelper<size_t>("size"), 3u);

            auto markOf = [&](const std::string& row) {
                return reqs->getEntity(row)->get<Prefab>()->getEntity("mark")->get<IconComponent>()->iconName;
            };
            EXPECT_EQ(markOf("row0"), "cross");   // 12 / 18, derived
            EXPECT_EQ(markOf("row1"), "cross");   // met: false
            EXPECT_EQ(markOf("row2"), "check");   // met: 1
            EXPECT_EQ(reqs->getEntity("value0")->get<TTFText>()->text, "12 / 18");

            EntityRef m = prefab->getEntity("ml");
            ASSERT_FALSE(m.empty());
            auto mlPrefab = m->get<Prefab>();
            EXPECT_EQ(mlPrefab->namedChildrenIds.count("mark"), 1u);
            EXPECT_EQ(mlPrefab->getEntity("label")->get<TTFText>()->text, "412");
            EXPECT_EQ(mlPrefab->getEntity("label")->get<TTFText>()->fontPath, "figure");
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, helpers_are_exposed_on_the_piece)
        {
            FactoriesFixture f;

            NodeSpec page = node("", "page");
            page.children.push_back(node("Panel", "panel", {{"width", 200.0f}, {"heading", "Parts"}}));
            page.children.push_back(node("StatLine", "stat", {{"value", 12}, {"max", 30}}));
            page.children.push_back(node("Gloss", "gloss", {{"text", "before"}, {"width", 200.0f}}));

            EntityRef root = buildTree(&f.ecs, page);
            auto prefab = root->get<Prefab>();
            f.settle();

            // The piece is a component on its root; the helpers change it in place.
            EntityRef panelEnt = prefab->getEntity("panel");
            ASSERT_TRUE(panelEnt->has<Panel>());
            EXPECT_EQ(panelEnt->get<Panel>()->spec.heading, "Parts");

            auto panel = panelEnt->get<Prefab>();
            EXPECT_TRUE(panel->hasHelper("setHeading"));
            EXPECT_TRUE(panel->hasHelper("setWidth"));
            panel->callHelper("setWidth", 240.0f);
            EXPECT_FLOAT_EQ(panelEnt->get<Panel>()->spec.width, 240.0f);

            EntityRef statEnt = prefab->getEntity("stat");
            auto stat = statEnt->get<Prefab>();
            EXPECT_TRUE(stat->hasHelper("setValue"));
            stat->callHelper("setValue", 20, false);
            EXPECT_EQ(statEnt->get<StatLine>()->spec.value, 20);

            auto gloss = prefab->getEntity("gloss")->get<Prefab>();
            ASSERT_TRUE(gloss->hasHelper("setText"));
            gloss->callHelper("setText", std::string("after"));
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, a_file_builds_end_to_end)
        {
            FactoriesFixture f;

            std::vector<std::string> errors;
            PrefabLoadOptions options;
            options.errors = &errors;

            auto spec = loadNodeSpec(&f.ecs, "ui/chronicle_panel.yaml", options);
            ASSERT_TRUE(spec.has_value());
            EXPECT_TRUE(errors.empty());

            EntityRef root = buildTree(&f.ecs, *spec);
            auto prefab = root->get<Prefab>();

            EntityRef panel = prefab->getEntity("panel");
            EntityRef fed = prefab->getEntity("fed");
            EntityRef train = prefab->getEntity("train");
            ASSERT_FALSE(panel.empty());
            ASSERT_FALSE(fed.empty());
            ASSERT_FALSE(train.empty());

            EXPECT_EQ(fed->get<Prefab>()->callHelper<size_t>("size"), 2u);
            EXPECT_FLOAT_EQ(fed->get<PositionComponent>()->width, 288.0f);   // the panel's inner width (320 - 2 x 16)
            EXPECT_EQ(train->get<Prefab>()->getEntity("face")->get<ButtonState>()->tag, "test.train");
            EXPECT_FLOAT_EQ(train->get<PositionComponent>()->z, 20.0f);     // the panel's content band

            f.settle();

            // x / y from the file placed the wrap.
            auto pos = root->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos->x, 10.0f);
            EXPECT_FLOAT_EQ(pos->y, 20.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, lifeclock_maps_props_and_records)
        {
            FactoriesFixture f;

            NodeSpec page = node("", "page");

            NodeSpec clockNode = node("LifeClock", "clock", {{"age", 17.4f}, {"runningMonths", 9.0f}, {"nextLabel", "Choose a path"}, {"nextIn", 7}});
            clockNode.records["milestones"] = {
                {{"age", 7}}, {{"age", 14}}, {{"age", 18}, {"label", "Of age"}}, {{"age", 25}}, {{"age", 40}}, {{"age", 43}},
            };
            clockNode.records["windows"] = {
                {{"from", 16}, {"to", 22}, {"label", "Squire"}},
                {{"from", 9}, {"to", 13}, {"label", "Choir"}, {"closed", true}},
            };
            page.children.push_back(clockNode);

            EntityRef root = buildTree(&f.ecs, page);
            EntityRef clockEnt = root->get<Prefab>()->getEntity("clock");
            ASSERT_FALSE(clockEnt.empty());
            ASSERT_TRUE(clockEnt->has<LifeClock>());

            const LifeClockSpec& spec = clockEnt->get<LifeClock>()->spec;
            EXPECT_FLOAT_EQ(spec.width, 640.0f);
            EXPECT_FLOAT_EQ(spec.startAge, 7.0f);
            EXPECT_FLOAT_EQ(spec.endAge, 43.0f);
            EXPECT_FLOAT_EQ(spec.age, 17.4f);
            EXPECT_FLOAT_EQ(spec.runningMonths, 9.0f);
            EXPECT_EQ(spec.nextLabel, "Choose a path");
            EXPECT_EQ(spec.nextIn, 7);
            ASSERT_EQ(spec.milestones.size(), 6u);
            EXPECT_FLOAT_EQ(spec.milestones[2].age, 18.0f);
            EXPECT_EQ(spec.milestones[2].label, "Of age");
            ASSERT_EQ(spec.windows.size(), 2u);
            EXPECT_FLOAT_EQ(spec.windows[0].from, 16.0f);
            EXPECT_FLOAT_EQ(spec.windows[0].to, 22.0f);
            EXPECT_EQ(spec.windows[0].label, "Squire");
            EXPECT_FALSE(spec.windows[0].closed);
            EXPECT_TRUE(spec.windows[1].closed);

            auto prefab = clockEnt->get<Prefab>();
            EXPECT_TRUE(prefab->hasHelper("setAge"));
            EXPECT_TRUE(prefab->hasHelper("setRunning"));
            EXPECT_TRUE(prefab->hasHelper("setNext"));
            EXPECT_TRUE(prefab->hasHelper("setWindowClosed"));

            prefab->callHelper("setRunning", 0.0f);
            EXPECT_FLOAT_EQ(clockEnt->get<LifeClock>()->spec.runningMonths, 0.0f);

            // The file builds the same clock.
            std::vector<std::string> errors;
            PrefabLoadOptions options;
            options.errors = &errors;

            auto fileSpec = loadNodeSpec(&f.ecs, "ui/lifeclock.yaml", options);
            ASSERT_TRUE(fileSpec.has_value());
            EXPECT_TRUE(errors.empty());

            EntityRef fileRoot = buildTree(&f.ecs, *fileSpec);
            EntityRef fileClock = fileRoot->get<Prefab>()->getEntity("clock");
            ASSERT_FALSE(fileClock.empty());
            ASSERT_TRUE(fileClock->has<LifeClock>());

            const LifeClockSpec& fromFile = fileClock->get<LifeClock>()->spec;
            EXPECT_FLOAT_EQ(fromFile.age, 17.4f);
            EXPECT_FLOAT_EQ(fromFile.runningMonths, 9.0f);
            EXPECT_EQ(fromFile.nextLabel, "Choose a path");
            EXPECT_EQ(fromFile.nextIn, 7);
            ASSERT_EQ(fromFile.milestones.size(), 6u);
            EXPECT_EQ(fromFile.milestones[2].label, "Of age");
            ASSERT_EQ(fromFile.windows.size(), 2u);
            EXPECT_EQ(fromFile.windows[1].label, "Choir");
            EXPECT_TRUE(fromFile.windows[1].closed);
        }
    }
}
