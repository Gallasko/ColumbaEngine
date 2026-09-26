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
#include "UI/gloss.h"
#include "UI/paint.h"
#include "Core/motion.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
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
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PrefabFactoryRegistry* registry = nullptr;
                TextStyles styles;

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
                    ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");
                    tip->setDefaultFont("chr-body-sm");
                    ecs.createSystem<GlossRegistry>(&tokens, &styles);
                    installIconEntries();

                    registry = ecs.createSystem<PrefabFactoryRegistry>();
                    registerEnginePrefabFactories(registry);
                    registerChronicleFactories(registry, &tokens, &styles);

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

            EXPECT_EQ(chronicleKinds().size(), 11u);
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

            auto built = buildTree(&f.ecs, spec);
            f.settle();

            auto* p = built.get<Panel>("p");
            auto* l = built.get<Label>("l");
            auto* r = built.get<RequirementList>("r");
            ASSERT_NE(p, nullptr);
            ASSERT_NE(l, nullptr);
            ASSERT_NE(r, nullptr);

            EXPECT_EQ(p->spec.z, 10);
            EXPECT_EQ(p->spec.contentZ, 20);

            // Neither child wrote a width or a z: both came from the panel.
            EXPECT_FLOAT_EQ(l->spec.width, p->innerWidth());
            EXPECT_EQ(l->spec.z, p->spec.contentZ);
            EXPECT_FLOAT_EQ(r->spec.width, p->innerWidth());
            EXPECT_EQ(r->spec.z, p->spec.contentZ);

            // Both went into the body layout, as Panel::addChild would have put them.
            auto body = p->body->get<VerticalLayout>();
            EXPECT_EQ(body->entities.size(), 2u);

            // The handle is the real result struct: its setters work.
            r->setItem(&f.ecs, f.styles, 0, 18, 18);
            EXPECT_TRUE(r->rows[0].item.isMet());
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, nested_panel_moves_to_the_next_band_by_itself)
        {
            FactoriesFixture f;

            NodeSpec outer = node("Panel", "outer", {{"width", 260.0f}, {"heading", "Who he is"}});
            NodeSpec inner = node("Panel", "inner", {{"heading", "Skills"}});
            inner.children.push_back(node("Label", "deep", {{"text", "Swordsmanship"}}));
            outer.children.push_back(inner);

            auto built = buildTree(&f.ecs, outer);

            auto* o = built.get<Panel>("outer");
            auto* i = built.get<Panel>("inner");
            auto* d = built.get<Label>("deep");
            ASSERT_NE(o, nullptr);
            ASSERT_NE(i, nullptr);
            ASSERT_NE(d, nullptr);

            EXPECT_EQ(i->spec.z, o->spec.contentZ);          // 20
            EXPECT_EQ(i->spec.contentZ, o->spec.contentZ + 10);   // 30
            EXPECT_FLOAT_EQ(i->spec.width, o->innerWidth());
            EXPECT_EQ(d->spec.z, i->spec.contentZ);
        }

        // ----------------------------------------------------------------------------------------
        TEST(chronicle_factories_test, enum_strings_colours_and_spacing_tokens)
        {
            FactoriesFixture f;

            NodeSpec page = node("", "page");
            page.children.push_back(node("Panel", "lit", {{"frame", "illuminated"}, {"width", 200.0f}}));
            page.children.push_back(node("Label", "ell", {{"text", "x"}, {"overflow", "ellipsis"}, {"align", "right"}, {"width", 100.0f}}));
            page.children.push_back(node("Label", "bad", {{"text", "x"}, {"colour", "not-a-token"}}));
            page.children.push_back(node("Button", "seal", {{"label", "Go"}, {"variant", "seal"}, {"tag", "t"}}));
            page.children.push_back(node("ProgressRule", "rule", {{"width", "space-7"}}));
            page.children.push_back(node("Mark", "mark", {{"name", "gold"}, {"size", 24}}));

            auto built = buildTree(&f.ecs, page);

            ASSERT_NE(built.get<Panel>("lit"), nullptr);
            EXPECT_EQ(built.get<Panel>("lit")->corners.size(), 4u);

            ASSERT_NE(built.get<Label>("ell"), nullptr);
            EXPECT_EQ(built.get<Label>("ell")->spec.overflow, Overflow::Ellipsis);
            EXPECT_EQ(built.get<Label>("ell")->spec.align, Align::Right);

            ASSERT_NE(built.get<Label>("bad"), nullptr);
            EXPECT_EQ(built.get<Label>("bad")->spec.colour, "ink");   // unknown token -> default

            ASSERT_NE(built.get<Button>("seal"), nullptr);
            EXPECT_EQ(built.get<Button>("seal")->spec.variant, ButtonVariant::Seal);
            EXPECT_EQ(built.get<Button>("seal")->spec.tag, "t");

            ASSERT_NE(built.get<ProgressRule>("rule"), nullptr);
            EXPECT_FLOAT_EQ(built.get<ProgressRule>("rule")->spec.width, 48.0f);   // space-7

            ASSERT_NE(built.get<Mark>("mark"), nullptr);
            EXPECT_EQ(built.get<Mark>("mark")->spec.size, MarkSize::S24);
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

            auto built = buildTree(&f.ecs, page);

            auto* t = built.get<Tabs>("tabs");
            ASSERT_NE(t, nullptr);
            EXPECT_EQ(t->tabs.size(), 3u);
            EXPECT_EQ(t->spec.items[1].badge, 2);
            EXPECT_EQ(t->spec.tag, "nav");

            auto* r = built.get<RequirementList>("reqs");
            ASSERT_NE(r, nullptr);
            ASSERT_EQ(r->rows.size(), 3u);
            EXPECT_EQ(r->rows[0].item.current, 12);
            EXPECT_EQ(r->rows[0].item.needed, 18);
            EXPECT_EQ(r->rows[0].item.met, -1);
            EXPECT_EQ(r->rows[1].item.met, 0);
            EXPECT_EQ(r->rows[2].item.met, 1);

            auto* m = built.get<MarkedLabel>("ml");
            ASSERT_NE(m, nullptr);
            EXPECT_TRUE(m->mark.has_value());
            EXPECT_EQ(m->label.spec.text, "412");
            EXPECT_EQ(m->label.spec.style, "figure");
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

            auto built = buildTree(&f.ecs, *spec);
            f.settle();

            auto* panel = built.get<Panel>("panel");
            auto* fed = built.get<RequirementList>("fed");
            auto* train = built.get<Button>("train");
            ASSERT_NE(panel, nullptr);
            ASSERT_NE(fed, nullptr);
            ASSERT_NE(train, nullptr);

            EXPECT_EQ(fed->rows.size(), 2u);
            EXPECT_FLOAT_EQ(fed->spec.width, panel->innerWidth());
            EXPECT_EQ(train->spec.tag, "test.train");
            EXPECT_EQ(train->spec.z, panel->spec.contentZ);

            // x / y from the file placed the wrap.
            auto pos = built.root->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos->x, 10.0f);
            EXPECT_FLOAT_EQ(pos->y, 20.0f);
        }
    }
}
