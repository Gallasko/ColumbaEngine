#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/prefab.h"
#include "UI/prefabspec.h"
#include "UI/prefabbuilder.h"
#include "UI/prefabfactory.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "ECS/entitysystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            // Spin up the minimum set of systems the builder needs: position resolves
            // anchors, prefab system handles SetMainEntityEvent. No render systems are
            // required because Simple2DObject / Texture2DComponent are plain components.
            void bootstrap(EntitySystem& ecs)
            {
                ecs.createSystem<PositionComponentSystem>();
                ecs.createSystem<PrefabSystem>();
                ecs.createSystem<PrefabFactoryRegistry>();
                ecs.succeed<PositionComponentSystem, PrefabSystem>();
            }

            NodeSpec shapeNode(const std::string& name, float w, float h,
                               float r = 255.0f, float g = 255.0f, float b = 255.0f, float a = 255.0f,
                               const std::string& shape = "Square")
            {
                NodeSpec n;
                n.kind = "Shape2D";
                n.name = name;
                n.props = {
                    {"shape",  shape},
                    {"width",  w},
                    {"height", h},
                    {"r",      r},
                    {"g",      g},
                    {"b",      b},
                    {"a",      a},
                };
                return n;
            }
        }

        // ----------------------------------------------------------------------------------------
        // buildNode: individual node realisation
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, build_shape2d_node_attaches_expected_components)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec;
            spec.kind = "Shape2D";
            spec.props = {
                {"shape",  std::string("Circle")},
                {"width",  64.0f},
                {"height", 32.0f},
                {"x",      10.0f},
                {"y",      20.0f},
                {"z",       3.0f},
                {"r",      10.0f},
                {"g",      20.0f},
                {"b",      30.0f},
                {"a",     200.0f},
            };

            auto ent = buildNode(&ecs, spec);

            ASSERT_FALSE(ent.empty());
            EXPECT_TRUE(ent->has<PositionComponent>());
            EXPECT_TRUE(ent->has<UiAnchor>());
            EXPECT_TRUE(ent->has<ViewportComponent>());
            EXPECT_TRUE(ent->has<Simple2DObject>());

            auto pos = ent->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos->x, 10.0f);
            EXPECT_FLOAT_EQ(pos->y, 20.0f);
            EXPECT_FLOAT_EQ(pos->z,  3.0f);
            EXPECT_FLOAT_EQ(pos->width,  64.0f);
            EXPECT_FLOAT_EQ(pos->height, 32.0f);

            auto shape = ent->get<Simple2DObject>();
            EXPECT_EQ(shape->shape, Shape2D::Circle);
            EXPECT_FLOAT_EQ(shape->colors.x,  10.0f);
            EXPECT_FLOAT_EQ(shape->colors.y,  20.0f);
            EXPECT_FLOAT_EQ(shape->colors.z,  30.0f);
            EXPECT_FLOAT_EQ(shape->colors.w, 200.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, build_texture_node_attaches_expected_components)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec;
            spec.kind = "Texture";
            spec.props = {
                {"texture", std::string("MyTex")},
                {"width",   48.0f},
                {"height",  16.0f},
                {"x",        5.0f},
                {"y",        7.0f},
            };

            auto ent = buildNode(&ecs, spec);

            ASSERT_FALSE(ent.empty());
            EXPECT_TRUE(ent->has<PositionComponent>());
            EXPECT_TRUE(ent->has<UiAnchor>());
            EXPECT_TRUE(ent->has<ViewportComponent>());
            EXPECT_TRUE(ent->has<Texture2DComponent>());

            auto pos = ent->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos->x, 5.0f);
            EXPECT_FLOAT_EQ(pos->y, 7.0f);
            EXPECT_FLOAT_EQ(pos->width,  48.0f);
            EXPECT_FLOAT_EQ(pos->height, 16.0f);

            EXPECT_EQ(ent->get<Texture2DComponent>()->textureName, "MyTex");
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, build_unknown_kind_returns_empty)
        {
            MockLogger<TerminalSink> logger;

            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec;
            spec.kind = "TotallyMadeUp";

            auto ent = buildNode(&ecs, spec);
            EXPECT_TRUE(ent.empty());
        }

        // ----------------------------------------------------------------------------------------
        // buildPrefab: composition + naming
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, build_prefab_main_only_registers_main_under_both_names)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 100.0f, 80.0f);

            auto container = buildPrefab(&ecs, spec);

            ASSERT_FALSE(container.empty());
            EXPECT_TRUE(container->has<Prefab>());
            EXPECT_TRUE(container->has<PositionComponent>());
            EXPECT_TRUE(container->has<UiAnchor>());

            ecs.executeOnce();

            auto prefab = container->get<Prefab>();

            // Main entity is registered under the auto "MainEntity" key and the spec name.
            auto byMainEntity = prefab->getEntity("MainEntity");
            auto byBg         = prefab->getEntity("bg");

            EXPECT_FALSE(byMainEntity.empty());
            EXPECT_FALSE(byBg.empty());
            EXPECT_EQ(byMainEntity.id, byBg.id);
            EXPECT_TRUE(byBg->has<Simple2DObject>());
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, build_prefab_with_children_registers_all_by_name)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 100.0f, 80.0f);
            spec.children.push_back(shapeNode("left",  10.0f, 10.0f));
            spec.children.push_back(shapeNode("right", 10.0f, 10.0f));
            // An unnamed child is added to the prefab but cannot be looked up by name.
            spec.children.push_back(shapeNode("", 5.0f, 5.0f));

            auto container = buildPrefab(&ecs, spec);

            ecs.executeOnce();

            auto prefab = container->get<Prefab>();

            // Main + 2 named children + 1 unnamed = 4 entities tracked on the prefab.
            EXPECT_EQ(prefab->childrenIds.size(), 4u);

            EXPECT_FALSE(prefab->getEntity("left").empty());
            EXPECT_FALSE(prefab->getEntity("right").empty());
            EXPECT_FALSE(prefab->getEntity("bg").empty());

            EXPECT_NE(prefab->getEntity("left").id, prefab->getEntity("right").id);
        }

        // ----------------------------------------------------------------------------------------
        // Build a prefab with anchored children and check that after one ECS tick all the
        // entities sit at the positions the spec asked for. This is the request's
        // "verify all entities are at the correct place" scenario.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, anchors_resolve_to_expected_positions_after_one_tick)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 200.0f, 100.0f);

            // Icon: anchored top-left of "main" with margins (15, 10).
            NodeSpec icon = shapeNode("icon", 20.0f, 20.0f);
            AnchorSpec topToMain;  topToMain.target  = "main"; topToMain.side  = AnchorType::Top;  topToMain.margin  = 10.0f;
            AnchorSpec leftToMain; leftToMain.target = "main"; leftToMain.side = AnchorType::Left; leftToMain.margin = 15.0f;
            icon.anchors = {topToMain, leftToMain};
            spec.children.push_back(icon);

            // Badge: anchored to icon's right side (badge.left = icon.right).
            NodeSpec badge = shapeNode("badge", 8.0f, 8.0f);
            AnchorSpec leftToIconRight;
            leftToIconRight.target     = "icon";
            leftToIconRight.side       = AnchorType::Left;
            leftToIconRight.targetSide = AnchorType::Right;
            AnchorSpec topToIcon; topToIcon.target = "icon"; topToIcon.side = AnchorType::Top;
            badge.anchors = {leftToIconRight, topToIcon};
            spec.children.push_back(badge);

            auto container = buildPrefab(&ecs, spec);
            auto containerPos = container->get<PositionComponent>();

            // Move the container into a non-zero spot so the assertions are not all zeros.
            containerPos->setX(50.0f);
            containerPos->setY(30.0f);

            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();

            auto bg    = prefab->getEntity("bg");
            auto icon2 = prefab->getEntity("icon");
            auto badge2 = prefab->getEntity("badge");

            ASSERT_FALSE(bg.empty());
            ASSERT_FALSE(icon2.empty());
            ASSERT_FALSE(badge2.empty());

            auto bgPos    = bg->get<PositionComponent>();
            auto iconPos  = icon2->get<PositionComponent>();
            auto badgePos = badge2->get<PositionComponent>();

            // Main entity is auto-anchored to the prefab's top-left and inherits its size.
            EXPECT_FLOAT_EQ(bgPos->x, 50.0f);
            EXPECT_FLOAT_EQ(bgPos->y, 30.0f);
            EXPECT_FLOAT_EQ(bgPos->width,  200.0f);
            EXPECT_FLOAT_EQ(bgPos->height, 100.0f);

            EXPECT_FLOAT_EQ(containerPos->width,  200.0f);
            EXPECT_FLOAT_EQ(containerPos->height, 100.0f);

            // Icon: top = bg.top + 10, left = bg.left + 15.
            EXPECT_FLOAT_EQ(iconPos->x, 50.0f + 15.0f);
            EXPECT_FLOAT_EQ(iconPos->y, 30.0f + 10.0f);
            EXPECT_FLOAT_EQ(iconPos->width,  20.0f);
            EXPECT_FLOAT_EQ(iconPos->height, 20.0f);

            // Badge: left = icon.right (=icon.x + icon.width), top = icon.top.
            EXPECT_FLOAT_EQ(badgePos->x, iconPos->x + iconPos->width);
            EXPECT_FLOAT_EQ(badgePos->y, iconPos->y);
            EXPECT_FLOAT_EQ(badgePos->width,  8.0f);
            EXPECT_FLOAT_EQ(badgePos->height, 8.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, center_in_target_centers_child_in_main)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 100.0f, 60.0f);

            NodeSpec dot = shapeNode("dot", 10.0f, 10.0f);
            dot.anchors = centerInAnchors("main");
            spec.children.push_back(dot);

            auto container = buildPrefab(&ecs, spec);

            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto dotPos = prefab->getEntity("dot")->get<PositionComponent>();
            auto bgPos  = prefab->getEntity("bg")->get<PositionComponent>();

            // Centered: dot center == bg center on both axes.
            EXPECT_FLOAT_EQ(dotPos->x + dotPos->width  / 2.0f, bgPos->x + bgPos->width  / 2.0f);
            EXPECT_FLOAT_EQ(dotPos->y + dotPos->height / 2.0f, bgPos->y + bgPos->height / 2.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, anchor_target_missing_is_skipped_without_throwing)
        {
            MockLogger<TerminalSink> logger;

            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 50.0f, 50.0f);

            NodeSpec ghost = shapeNode("ghost", 10.0f, 10.0f);
            AnchorSpec dangling; dangling.target = "doesNotExist"; dangling.side = AnchorType::Top;
            ghost.anchors = {dangling};
            spec.children.push_back(ghost);

            auto container = buildPrefab(&ecs, spec);
            ASSERT_FALSE(container.empty());

            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto ghostEnt = prefab->getEntity("ghost");
            ASSERT_FALSE(ghostEnt.empty());

            // No anchor target -> entity keeps its (default) position. We mostly want to
            // verify the builder did not crash.
            auto pos = ghostEnt->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos->width,  10.0f);
            EXPECT_FLOAT_EQ(pos->height, 10.0f);
        }

        // ----------------------------------------------------------------------------------------
        // PrefabFactoryRegistry: registering, listing, and invoking factories.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, factory_registration_and_listing)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            auto* registry = ecs.getSystem<PrefabFactoryRegistry>();
            ASSERT_NE(registry, nullptr);

            EXPECT_FALSE(registry->hasFactory("Tag"));

            ParamSchema schema;
            schema.entries = {
                {"label", ElementType::UnionType::STRING, ElementType{std::string("default")}},
                {"size",  ElementType::UnionType::FLOAT,  ElementType{24.0f}},
            };

            registry->registerFactory("Tag", std::move(schema),
                [](EntitySystem* e, const PrefabParams& p) -> EntityRef {
                    auto cl = makeUiSimple2DShape(e, Shape2D::Square,
                                                  getParamFloat(p, "size"),
                                                  getParamFloat(p, "size"));
                    return cl.entity;
                });

            EXPECT_TRUE(registry->hasFactory("Tag"));
            EXPECT_FALSE(registry->hasFactory("OtherTag"));

            auto names = registry->listFactories();
            EXPECT_NE(std::find(names.begin(), names.end(), "Tag"), names.end());

            const auto& sch = registry->schemaOf("Tag");
            ASSERT_EQ(sch.entries.size(), 2u);
            EXPECT_EQ(sch.entries[0].name, "label");
            EXPECT_EQ(sch.entries[1].name, "size");
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, factory_invoked_directly_returns_built_entity)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            auto* registry = ecs.getSystem<PrefabFactoryRegistry>();
            ASSERT_NE(registry, nullptr);

            registry->registerFactory("Box", ParamSchema{},
                [](EntitySystem* e, const PrefabParams& p) -> EntityRef {
                    const float w = getParamFloat(p, "width", 0.0f);
                    const float h = getParamFloat(p, "height", 0.0f);
                    auto cl = makeUiSimple2DShape(e, Shape2D::Square, w, h);
                    return cl.entity;
                });

            PrefabParams params;
            params["width"]  = 40.0f;
            params["height"] = 12.0f;

            auto ent = registry->build("Box", params);
            ASSERT_FALSE(ent.empty());

            auto pos = ent->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos->width,  40.0f);
            EXPECT_FLOAT_EQ(pos->height, 12.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, factory_missing_returns_empty_entity)
        {
            MockLogger<TerminalSink> logger;

            EntitySystem ecs;
            bootstrap(ecs);

            auto* registry = ecs.getSystem<PrefabFactoryRegistry>();
            ASSERT_NE(registry, nullptr);

            auto ent = registry->build("NotRegistered");
            EXPECT_TRUE(ent.empty());
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, factory_defaults_apply_when_param_missing)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            auto* registry = ecs.getSystem<PrefabFactoryRegistry>();
            ASSERT_NE(registry, nullptr);

            ParamSchema schema;
            schema.entries = {
                {"width",  ElementType::UnionType::FLOAT, ElementType{77.0f}},
                {"height", ElementType::UnionType::FLOAT, ElementType{33.0f}},
            };

            registry->registerFactory("Slab", std::move(schema),
                [](EntitySystem* e, const PrefabParams& p) -> EntityRef {
                    auto cl = makeUiSimple2DShape(e, Shape2D::Square,
                                                  getParamFloat(p, "width"),
                                                  getParamFloat(p, "height"));
                    return cl.entity;
                });

            // Build with no params - the schema defaults should be merged in.
            auto ent = registry->build("Slab");
            ASSERT_FALSE(ent.empty());

            auto pos = ent->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos->width,  77.0f);
            EXPECT_FLOAT_EQ(pos->height, 33.0f);

            // Caller-provided values override the defaults.
            PrefabParams override;
            override["width"] = 5.0f;

            auto ent2 = registry->build("Slab", override);
            auto pos2 = ent2->get<PositionComponent>();
            EXPECT_FLOAT_EQ(pos2->width,  5.0f);
            EXPECT_FLOAT_EQ(pos2->height, 33.0f);  // default kept
        }

        // ----------------------------------------------------------------------------------------
        // buildNode dispatches "Factory:<name>" through the registry.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, build_node_dispatches_factory_prefix_to_registry)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            auto* registry = ecs.getSystem<PrefabFactoryRegistry>();
            ASSERT_NE(registry, nullptr);

            bool factoryCalled = false;
            ParamSchema schema;
            schema.entries = {
                {"label", ElementType::UnionType::STRING, ElementType{std::string("def")}},
            };

            registry->registerFactory("Pill", std::move(schema),
                [&factoryCalled](EntitySystem* e, const PrefabParams& p) -> EntityRef {
                    factoryCalled = true;
                    EXPECT_EQ(getParamString(p, "label"), "hello");
                    auto cl = makeUiSimple2DShape(e, Shape2D::Circle, 20.0f, 20.0f);
                    return cl.entity;
                });

            NodeSpec spec;
            spec.kind  = "Factory:Pill";
            spec.props = {{"label", std::string("hello")}};

            auto ent = buildNode(&ecs, spec);

            EXPECT_TRUE(factoryCalled);
            ASSERT_FALSE(ent.empty());
            EXPECT_TRUE(ent->has<Simple2DObject>());
            EXPECT_EQ(ent->get<Simple2DObject>()->shape, Shape2D::Circle);
        }

        // ----------------------------------------------------------------------------------------
        // A prefab whose main node is itself produced by a custom factory: this exercises
        // both the registry path and SetMainEntityEvent wiring on the result.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, build_prefab_with_factory_main_resolves_positions)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            auto* registry = ecs.getSystem<PrefabFactoryRegistry>();
            ASSERT_NE(registry, nullptr);

            registry->registerFactory("Card", ParamSchema{},
                [](EntitySystem* e, const PrefabParams& p) -> EntityRef {
                    const float w = getParamFloat(p, "width",  120.0f);
                    const float h = getParamFloat(p, "height",  40.0f);
                    auto cl = makeUiSimple2DShape(e, Shape2D::Square, w, h);
                    return cl.entity;
                });

            PrefabSpec spec;
            spec.mainNode.kind  = "Factory:Card";
            spec.mainNode.name  = "card";
            spec.mainNode.props = {{"width", 120.0f}, {"height", 40.0f}};

            auto container = buildPrefab(&ecs, spec);
            auto containerPos = container->get<PositionComponent>();
            containerPos->setX(8.0f);
            containerPos->setY(4.0f);

            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto card = prefab->getEntity("card");
            ASSERT_FALSE(card.empty());

            auto cardPos = card->get<PositionComponent>();
            EXPECT_FLOAT_EQ(cardPos->x, 8.0f);
            EXPECT_FLOAT_EQ(cardPos->y, 4.0f);
            EXPECT_FLOAT_EQ(cardPos->width,  120.0f);
            EXPECT_FLOAT_EQ(cardPos->height,  40.0f);

            EXPECT_FLOAT_EQ(containerPos->width,  120.0f);
            EXPECT_FLOAT_EQ(containerPos->height,  40.0f);
        }
    }
}
