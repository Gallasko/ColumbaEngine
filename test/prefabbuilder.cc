#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/prefab.h"
#include "UI/prefabspec.h"
#include "UI/prefabbuilder.h"
#include "UI/prefabfactory.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "ECS/entitysystem.h"
#include "Systems/coresystems.h"

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

        // ----------------------------------------------------------------------------------------
        // Nested PrefabSpec as a child: a complete sub-tree dropped into the outer spec's
        // children list. The nested container entity is registered by name on the outer Prefab
        // and can be anchored to like any other named child.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, nested_prefab_registers_as_named_child_of_outer)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec inner;
            inner.mainNode = shapeNode("innerBg", 40.0f, 30.0f);
            inner.name     = "machineA";

            PrefabSpec outer;
            outer.mainNode = shapeNode("outerBg", 200.0f, 100.0f);
            outer.children.push_back(std::move(inner));

            auto container = buildPrefab(&ecs, outer);
            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto machineEnt = prefab->getEntity("machineA");
            ASSERT_FALSE(machineEnt.empty());
            EXPECT_TRUE(machineEnt->has<Prefab>());
            EXPECT_TRUE(machineEnt->has<UiAnchor>());
            EXPECT_TRUE(machineEnt->has<PositionComponent>());

            // The nested container adopts its own mainNode's size.
            auto machinePos = machineEnt->get<PositionComponent>();
            EXPECT_FLOAT_EQ(machinePos->width,  40.0f);
            EXPECT_FLOAT_EQ(machinePos->height, 30.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, outer_leaf_can_anchor_to_nested_prefab_sibling)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec inner;
            inner.mainNode = shapeNode("innerBg", 50.0f, 20.0f);
            inner.name     = "header";
            // Anchor the nested container to the outer main entity's top-left so it has a fixed position.
            inner.anchors  = {
                AnchorSpec{"main", AnchorType::Top,  0.0f},
                AnchorSpec{"main", AnchorType::Left, 0.0f},
            };

            NodeSpec body = shapeNode("body", 50.0f, 30.0f);
            // body anchored just below the nested header: body.top = header.bottom.
            body.anchors = {
                AnchorSpec{"header", AnchorType::Top, AnchorType::Bottom, 0.0f},
                AnchorSpec{"header", AnchorType::Left, AnchorType::Left, 0.0f},
            };

            PrefabSpec outer;
            outer.mainNode = shapeNode("outerBg", 100.0f, 100.0f);
            outer.children.push_back(std::move(inner));
            outer.children.push_back(body);

            auto container = buildPrefab(&ecs, outer);
            auto containerPos = container->get<PositionComponent>();
            containerPos->setX(5.0f);
            containerPos->setY(7.0f);

            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto headerPos = prefab->getEntity("header")->get<PositionComponent>();
            auto bodyPos   = prefab->getEntity("body")->get<PositionComponent>();

            // header sits at the outer main's top-left, which == container position.
            EXPECT_FLOAT_EQ(headerPos->x, 5.0f);
            EXPECT_FLOAT_EQ(headerPos->y, 7.0f);

            // body anchored to header.bottom (header.y + header.height = 7 + 20 = 27).
            EXPECT_FLOAT_EQ(bodyPos->x, headerPos->x);
            EXPECT_FLOAT_EQ(bodyPos->y, headerPos->y + headerPos->height);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, nested_prefab_centerInAnchors_helper_centers_on_outer_main)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec inner;
            inner.mainNode = shapeNode("innerBg", 20.0f, 20.0f);
            inner.name     = "blob";
            inner.anchors  = centerInAnchors("main");

            PrefabSpec outer;
            outer.mainNode = shapeNode("outerBg", 100.0f, 80.0f);
            outer.children.push_back(std::move(inner));

            auto container = buildPrefab(&ecs, outer);
            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto outerBg = prefab->getEntity("outerBg")->get<PositionComponent>();
            auto blob    = prefab->getEntity("blob")->get<PositionComponent>();

            EXPECT_FLOAT_EQ(blob->x + blob->width  / 2.0f, outerBg->x + outerBg->width  / 2.0f);
            EXPECT_FLOAT_EQ(blob->y + blob->height / 2.0f, outerBg->y + outerBg->height / 2.0f);
        }

        // ----------------------------------------------------------------------------------------
        // Inner names do not leak into outer lookup: each buildPrefab call has its own scope.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, inner_prefab_names_stay_scoped)
        {
            MockLogger<TerminalSink> logger;

            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec inner;
            inner.mainNode = shapeNode("inside", 10.0f, 10.0f);
            inner.name     = "nested";

            PrefabSpec outer;
            outer.mainNode = shapeNode("outerBg", 50.0f, 50.0f);
            outer.children.push_back(std::move(inner));

            auto container = buildPrefab(&ecs, outer);
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            // "nested" is the outer-visible handle for the inner container.
            EXPECT_FALSE(prefab->getEntity("nested").empty());
            // "inside" was the inner main's name — must NOT bubble up to the outer prefab.
            EXPECT_TRUE(prefab->getEntity("inside").empty());
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, two_levels_deep_nesting_resolves)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec c;
            c.mainNode = shapeNode("cBg", 8.0f, 8.0f);
            c.name     = "C";

            PrefabSpec b;
            b.mainNode = shapeNode("bBg", 30.0f, 30.0f);
            b.name     = "B";
            b.children.push_back(std::move(c));

            PrefabSpec a;
            a.mainNode = shapeNode("aBg", 80.0f, 80.0f);
            a.children.push_back(std::move(b));

            auto container = buildPrefab(&ecs, a);
            ecs.executeOnce();

            auto outer = container->get<Prefab>();
            auto bEnt  = outer->getEntity("B");
            ASSERT_FALSE(bEnt.empty());
            ASSERT_TRUE(bEnt->has<Prefab>());

            auto cEnt = bEnt->get<Prefab>()->getEntity("C");
            ASSERT_FALSE(cEnt.empty());
            EXPECT_TRUE(cEnt->has<Prefab>());

            // Outer can't see C — only B.
            EXPECT_TRUE(outer->getEntity("C").empty());
        }

        // ----------------------------------------------------------------------------------------
        // AnchorSpec.targetId resolves the anchor by entity id, bypassing the name map entirely.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, root_anchor_resolves_via_targetId)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            // An entity outside the prefab — only the id ties it to the spec.
            auto external = makeUiSimple2DShape(&ecs, Shape2D::Square, 40.0f, 30.0f);
            external.get<PositionComponent>()->setX(100.0f);
            external.get<PositionComponent>()->setY(50.0f);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 20.0f, 20.0f);
            spec.anchors  = {
                AnchorSpec{external.entity.id, AnchorType::Top,  AnchorType::Bottom, 0.0f},
                AnchorSpec{external.entity.id, AnchorType::Left, AnchorType::Left,   0.0f},
            };

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();
            ecs.executeOnce();

            auto bg = container->get<Prefab>()->getEntity("bg")->get<PositionComponent>();
            // bg follows the container, which is anchored to external.bottom / external.left.
            EXPECT_FLOAT_EQ(bg->x, 100.0f);                      // external.left
            EXPECT_FLOAT_EQ(bg->y, 50.0f + 30.0f);               // external.bottom = y + height
        }

        // ----------------------------------------------------------------------------------------
        // Anchor resolution falls through to EntityNameSystem when the target name is not in the
        // prefab's local sibling scope.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, root_anchor_resolves_via_entityname_system)
        {
            EntitySystem ecs;
            ecs.createSystem<PositionComponentSystem>();
            ecs.createSystem<PrefabSystem>();
            ecs.createSystem<PrefabFactoryRegistry>();
            ecs.createSystem<EntityNameSystem>();
            ecs.succeed<PositionComponentSystem, PrefabSystem>();

            auto external = makeUiSimple2DShape(&ecs, Shape2D::Square, 40.0f, 30.0f);
            external.get<PositionComponent>()->setX(200.0f);
            external.get<PositionComponent>()->setY(80.0f);
            ecs.attach<EntityName>(external.entity, std::string("globalAnchor"));

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 20.0f, 20.0f);
            // No targetId; name should resolve through EntityNameSystem.
            spec.anchors  = {
                AnchorSpec{std::string("globalAnchor"), AnchorType::Top,  AnchorType::Top,  0.0f},
                AnchorSpec{std::string("globalAnchor"), AnchorType::Left, AnchorType::Left, 0.0f},
            };

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();
            ecs.executeOnce();

            auto bg = container->get<Prefab>()->getEntity("bg")->get<PositionComponent>();
            EXPECT_FLOAT_EQ(bg->x, 200.0f);
            EXPECT_FLOAT_EQ(bg->y, 80.0f);
        }

        // ----------------------------------------------------------------------------------------
        // targetId wins over target when both are set (explicit-form precedence).
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, targetId_overrides_target_name)
        {
            MockLogger<TerminalSink> logger;

            EntitySystem ecs;
            bootstrap(ecs);

            auto byId = makeUiSimple2DShape(&ecs, Shape2D::Square, 10.0f, 10.0f);
            byId.get<PositionComponent>()->setX(500.0f);
            byId.get<PositionComponent>()->setY(0.0f);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 20.0f, 20.0f);

            // target = "main" would normally resolve to the prefab's own main entity at (0,0).
            // Setting targetId to the external entity must override that.
            AnchorSpec a;
            a.target     = "main";
            a.targetId   = byId.entity.id;
            a.side       = AnchorType::Left;
            a.targetSide = AnchorType::Left;
            spec.anchors = {a};

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();
            ecs.executeOnce();

            auto bg = container->get<Prefab>()->getEntity("bg")->get<PositionComponent>();
            EXPECT_FLOAT_EQ(bg->x, 500.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, reserved_name_on_nested_prefab_is_refused)
        {
            MockLogger<TerminalSink> logger;

            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec inner;
            inner.mainNode = shapeNode("inside", 10.0f, 10.0f);
            inner.name     = "main";   // reserved

            PrefabSpec outer;
            outer.mainNode = shapeNode("outerBg", 50.0f, 50.0f);
            outer.children.push_back(std::move(inner));

            auto container = buildPrefab(&ecs, outer);
            ecs.executeOnce();

            // "main" resolves to the OUTER prefab's main entity, not the nested one.
            auto mainEnt = container->get<Prefab>()->getEntity("MainEntity");
            ASSERT_FALSE(mainEnt.empty());
            EXPECT_FALSE(mainEnt->has<Prefab>());  // outer main is a Shape2D, not a sub-prefab
        }

        // ----------------------------------------------------------------------------------------
        // Empty anchor (no id, no name) is skipped silently with no log spam.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, empty_anchor_target_is_skipped)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 10.0f, 10.0f);
            AnchorSpec a;  // default-constructed: target empty, targetId 0
            a.side = AnchorType::Top;
            spec.anchors = {a};

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();

            // No assertion on position; the test passes if the build did not crash and the
            // empty anchor was treated as a no-op.
            EXPECT_FALSE(container.empty());
        }

        // ----------------------------------------------------------------------------------------
        // Flow: PrefabSpec::flow auto-anchors children with empty user-anchors.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, horizontal_flow_chains_children_left_to_right)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 400.0f, 50.0f);
            spec.flow     = Flow::Horizontal;
            spec.padding  = 3.0f;
            spec.spacing  = 5.0f;
            spec.children.push_back(shapeNode("a", 40.0f, 20.0f));
            spec.children.push_back(shapeNode("b", 30.0f, 20.0f));
            spec.children.push_back(shapeNode("c", 25.0f, 20.0f));

            auto container = buildPrefab(&ecs, spec);
            auto cPos = container->get<PositionComponent>();
            cPos->setX(10.0f);
            cPos->setY(20.0f);

            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto a = prefab->getEntity("a")->get<PositionComponent>();
            auto b = prefab->getEntity("b")->get<PositionComponent>();
            auto c = prefab->getEntity("c")->get<PositionComponent>();

            // Cross-axis: each child sits at main.top + padding.
            EXPECT_FLOAT_EQ(a->y, 20.0f + 3.0f);
            EXPECT_FLOAT_EQ(b->y, 20.0f + 3.0f);
            EXPECT_FLOAT_EQ(c->y, 20.0f + 3.0f);

            // Main-axis: first at main.left + padding, then chained off previous .right + spacing.
            EXPECT_FLOAT_EQ(a->x, 10.0f + 3.0f);
            EXPECT_FLOAT_EQ(b->x, a->x + a->width + 5.0f);
            EXPECT_FLOAT_EQ(c->x, b->x + b->width + 5.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, vertical_flow_chains_children_top_to_bottom)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 80.0f, 400.0f);
            spec.flow     = Flow::Vertical;
            spec.padding  = 2.0f;
            spec.spacing  = 4.0f;
            spec.children.push_back(shapeNode("row1", 60.0f, 30.0f));
            spec.children.push_back(shapeNode("row2", 60.0f, 15.0f));
            spec.children.push_back(shapeNode("row3", 60.0f, 22.0f));

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto r1 = prefab->getEntity("row1")->get<PositionComponent>();
            auto r2 = prefab->getEntity("row2")->get<PositionComponent>();
            auto r3 = prefab->getEntity("row3")->get<PositionComponent>();

            // Cross-axis: each row sits at main.left + padding.
            EXPECT_FLOAT_EQ(r1->x, 2.0f);
            EXPECT_FLOAT_EQ(r2->x, 2.0f);
            EXPECT_FLOAT_EQ(r3->x, 2.0f);

            // Main-axis: first at main.top + padding, then chained off previous .bottom + spacing.
            EXPECT_FLOAT_EQ(r1->y, 2.0f);
            EXPECT_FLOAT_EQ(r2->y, r1->y + r1->height + 4.0f);
            EXPECT_FLOAT_EQ(r3->y, r2->y + r2->height + 4.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, flow_none_preserves_current_behaviour)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 100.0f, 100.0f);
            spec.flow     = Flow::None;       // default; spelled out for clarity
            spec.padding  = 99.0f;            // ignored when flow == None
            spec.spacing  = 99.0f;
            spec.children.push_back(shapeNode("a", 10.0f, 10.0f));
            spec.children.push_back(shapeNode("b", 10.0f, 10.0f));

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto a = prefab->getEntity("a")->get<PositionComponent>();
            auto b = prefab->getEntity("b")->get<PositionComponent>();

            // Without flow, no anchors are synthesised — children sit at (0, 0).
            EXPECT_FLOAT_EQ(a->x, 0.0f);
            EXPECT_FLOAT_EQ(a->y, 0.0f);
            EXPECT_FLOAT_EQ(b->x, 0.0f);
            EXPECT_FLOAT_EQ(b->y, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // Manually anchored children are transparent to the flow: the chain continues from
        // the previous IN-FLOW sibling, skipping the manual one (matches CSS "out of flow").
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, manual_anchor_child_is_transparent_to_flow_chain)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 400.0f, 50.0f);
            spec.flow     = Flow::Horizontal;
            spec.padding  = 0.0f;
            spec.spacing  = 0.0f;

            spec.children.push_back(shapeNode("flowA", 20.0f, 20.0f));   // in flow

            // Manually anchored: stuck to main.right; must NOT break the chain.
            NodeSpec manual = shapeNode("manual", 8.0f, 8.0f);
            manual.anchors = {
                AnchorSpec{std::string("main"), AnchorType::Right, AnchorType::Right, 0.0f},
                AnchorSpec{std::string("main"), AnchorType::Top,   0.0f},
            };
            spec.children.push_back(manual);

            spec.children.push_back(shapeNode("flowB", 30.0f, 20.0f));   // chain off flowA, not manual

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto a = prefab->getEntity("flowA")->get<PositionComponent>();
            auto b = prefab->getEntity("flowB")->get<PositionComponent>();

            // flowB should sit immediately right of flowA (chain skips manual).
            EXPECT_FLOAT_EQ(b->x, a->x + a->width);
            EXPECT_FLOAT_EQ(b->y, a->y);
        }

        // ----------------------------------------------------------------------------------------
        // Single flow child still gets the first-child anchors.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, single_flow_child_anchors_to_main_top_left)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec spec;
            spec.mainNode = shapeNode("bg", 100.0f, 100.0f);
            spec.flow     = Flow::Horizontal;
            spec.padding  = 7.0f;
            spec.children.push_back(shapeNode("only", 10.0f, 10.0f));

            auto container = buildPrefab(&ecs, spec);
            ecs.executeOnce();

            auto only = container->get<Prefab>()->getEntity("only")->get<PositionComponent>();
            EXPECT_FLOAT_EQ(only->x, 7.0f);
            EXPECT_FLOAT_EQ(only->y, 7.0f);
        }

        // ----------------------------------------------------------------------------------------
        // Nested PrefabSpec children participate in flow just like NodeSpec children.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_builder_test, nested_prefabspec_participates_in_outer_flow)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            PrefabSpec inner;
            inner.mainNode = shapeNode("innerBg", 40.0f, 20.0f);
            inner.name     = "nested";
            // inner.anchors deliberately empty — so the OUTER flow controls its placement.

            PrefabSpec outer;
            outer.mainNode = shapeNode("outerBg", 200.0f, 50.0f);
            outer.flow     = Flow::Horizontal;
            outer.padding  = 1.0f;
            outer.spacing  = 2.0f;
            outer.children.push_back(shapeNode("leaf1", 30.0f, 20.0f));
            outer.children.push_back(std::move(inner));
            outer.children.push_back(shapeNode("leaf2", 15.0f, 20.0f));

            auto container = buildPrefab(&ecs, outer);
            ecs.executeOnce();
            ecs.executeOnce();

            auto prefab = container->get<Prefab>();
            auto leaf1  = prefab->getEntity("leaf1")->get<PositionComponent>();
            auto nested = prefab->getEntity("nested")->get<PositionComponent>();
            auto leaf2  = prefab->getEntity("leaf2")->get<PositionComponent>();

            // leaf1 -> main.left + padding; nested -> leaf1.right + spacing; leaf2 -> nested.right + spacing.
            EXPECT_FLOAT_EQ(leaf1->x,  1.0f);
            EXPECT_FLOAT_EQ(nested->x, leaf1->x + leaf1->width + 2.0f);
            EXPECT_FLOAT_EQ(leaf2->x,  nested->x + nested->width + 2.0f);
        }
    }
}
