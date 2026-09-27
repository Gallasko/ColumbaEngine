#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/prefab.h"
#include "UI/prefabspec.h"
#include "UI/prefabbuilder.h"
#include "UI/prefabfactory.h"
#include "UI/enginefactories.h"
#include "UI/sizer.h"
#include "2D/simple2dobject.h"
#include "ECS/entitysystem.h"
#include "Systems/coresystems.h"

#include "mocklogger.h"

// buildTree: the factory contract (slot, childDefaults, records), the wrap placement and the
// layout-root wrap, exercised with engine-only test kinds so no game kit is needed.
namespace pg
{
    namespace test
    {
        namespace
        {
            // The slot layout of the last "Box" built, so a test can look into it.
            EntityRef lastBoxLayout;

            // Keeps every message so a test can count the builder's own diagnostics rather
            // than whatever else the engine logs while entities are created.
            struct CaptureSink : public Logger::LogSink
            {
                static std::vector<std::string>& messages()
                {
                    static std::vector<std::string> m;
                    return m;
                }

                virtual void processLog(const Logger::Info& log) override
                {
                    messages().push_back(log.message);
                }
            };

            size_t countLogs(const std::string& needle)
            {
                size_t n = 0;
                for (const auto& m : CaptureSink::messages())
                    if (m.find(needle) != std::string::npos)
                        ++n;
                return n;
            }

            std::string allLogs()
            {
                std::string s;
                for (const auto& m : CaptureSink::messages())
                    s += m + "\n";
                return s;
            }

            void bootstrap(EntitySystem& ecs)
            {
                ecs.createSystem<PositionComponentSystem>();
                ecs.createSystem<LayoutSystem>();
                ecs.createSystem<PrefabSystem>();
                ecs.succeed<PositionComponentSystem, LayoutSystem>();
                ecs.succeed<PositionComponentSystem, PrefabSystem>();

                auto* registry = ecs.createSystem<PrefabFactoryRegistry>();
                registerEnginePrefabFactories(registry);

                // "Box": a backdrop with a vertical layout inside (the slot), handing its inner
                // width and a z band down to its children — the shape of a panel, engine-only.
                {
                    ParamSchema schema;
                    schema.entries = {{"width", 100.0f}, {"height", 50.0f}, {"inner", 80.0f}, {"z", 0.0f}};

                    registry->registerFactory("Box", std::move(schema),
                        PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                        {
                            auto* reg = ecs->getSystem<PrefabFactoryRegistry>();
                            const float inner = getParamFloat(spec.props, "inner");
                            const float z = getParamFloat(spec.props, "z");

                            EntityRef back = reg->build("Shape2D", {
                                {"width",  getParamFloat(spec.props, "width")},
                                {"height", getParamFloat(spec.props, "height")},
                                {"z",      z},
                            });

                            auto layout = makeVerticalLayout(ecs, 0.0f, 0.0f, inner, 0.0f);
                            layout.get<PositionComponent>()->setZ(z + 5.0f);
                            auto la = layout.get<UiAnchor>();
                            la->setTopAnchor(PosAnchor{back.id, AnchorType::Top});
                            la->setTopMargin(10.0f);
                            la->setLeftAnchor(PosAnchor{back.id, AnchorType::Left});
                            la->setLeftMargin(10.0f);

                            lastBoxLayout = layout.entity;

                            FactoryResult r;
                            r.entity = back;
                            r.slot = layout.entity;
                            r.childDefaults = {{"width", inner}, {"z", z + 5.0f}};
                            return r;
                        }});
                }

                // "Probe": a shape sized from the props it received — shows what a child
                // actually gets after inheritance and schema merging.
                {
                    ParamSchema schema;
                    schema.entries = {{"width", 1.0f}, {"height", 1.0f}, {"z", 0.0f}};

                    registry->registerFactory("Probe", std::move(schema),
                        PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                        {
                            auto* reg = ecs->getSystem<PrefabFactoryRegistry>();
                            FactoryResult r;
                            r.entity = reg->build("Shape2D", {
                                {"width",  getParamFloat(spec.props, "width")},
                                {"height", getParamFloat(spec.props, "height")},
                                {"z",      getParamFloat(spec.props, "z")},
                            });
                            return r;
                        }});
                }

                // "Rec": a shape as wide as the number of `items` records it received.
                {
                    registry->registerFactory("Rec", ParamSchema{},
                        PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                        {
                            auto* reg = ecs->getSystem<PrefabFactoryRegistry>();
                            size_t n = 0;
                            auto it = spec.records.find("items");
                            if (it != spec.records.end())
                                n = it->second.size();

                            FactoryResult r;
                            r.entity = reg->build("Shape2D", {{"width", static_cast<float>(n)}, {"height", 1.0f}});
                            return r;
                        }});
                }

                // "Need": a kind with a Required param and no default for it.
                {
                    ParamSchema schema;
                    schema.entries = {
                        {"label", UnionType::STRING, ParamSchema::Requirement::Required},
                        {"width", 4.0f},
                    };

                    registry->registerFactory("Need", std::move(schema),
                        PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                        {
                            auto* reg = ecs->getSystem<PrefabFactoryRegistry>();
                            FactoryResult r;
                            r.entity = reg->build("Shape2D", {{"width", getParamFloat(spec.props, "width")}, {"height", 1.0f}});
                            return r;
                        }});
                }

                // "BadSlot": returns a plain shape (no layout component) as its slot.
                {
                    ParamSchema schema;
                    schema.entries = {{"width", 100.0f}, {"height", 50.0f}};

                    registry->registerFactory("BadSlot", std::move(schema),
                        PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                        {
                            auto* reg = ecs->getSystem<PrefabFactoryRegistry>();
                            FactoryResult r;
                            r.entity = reg->build("Shape2D", {{"width", getParamFloat(spec.props, "width")}, {"height", getParamFloat(spec.props, "height")}});
                            r.slot   = reg->build("Shape2D", {{"width", 1.0f}, {"height", 1.0f}});
                            return r;
                        }});
                }
            }

            // The child wraps of a built prefab (everything tracked on it but the main entity).
            std::vector<EntityRef> childWraps(EntitySystem& ecs, EntityRef root)
            {
                std::vector<EntityRef> out;
                auto prefab = root->get<Prefab>();
                const _unique_id mainId = prefab->getEntity("MainEntity").id;

                for (const auto& id : prefab->childrenIds)
                {
                    auto ent = ecs.getEntity(id);
                    if (ent and ent->id != mainId)
                        out.push_back(ent);
                }

                return out;
            }

            NodeSpec node(const std::string& kind, const std::string& name, ElementMap props = {})
            {
                NodeSpec n;
                n.kind = kind;
                n.name = name;
                n.props = std::move(props);
                return n;
            }

            NodeSpec shapeNode(const std::string& name, float w, float h)
            {
                return node("Shape2D", name, {{"width", w}, {"height", h}});
            }

            void settle(EntitySystem& ecs)
            {
                ecs.executeOnce();
                ecs.executeOnce();
            }

            // The leaf a name resolves to on the root's prefab.
            EntityRef leafOf(EntityRef root, const std::string& name)
            {
                return root->get<Prefab>()->getEntity(name);
            }
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, build_tree_returns_a_prefab_root)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            EntityRef root = buildTree(&ecs, node("Box", "box"));

            ASSERT_FALSE(root.empty());
            ASSERT_TRUE(root->has<Prefab>());
            EXPECT_FALSE(leafOf(root, "MainEntity").empty());
            EXPECT_FALSE(leafOf(root, "box").empty());
            EXPECT_TRUE(leafOf(root, "box")->has<Simple2DObject>());

            // An unnamed node builds the same way; it is just not named on its prefab.
            EntityRef anon = buildTree(&ecs, node("Box", ""));
            ASSERT_FALSE(anon.empty());
            EXPECT_EQ(anon->get<Prefab>()->namedChildrenIds.size(), 1u);   // MainEntity only
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, slot_children_go_into_the_factory_layout)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec = node("Box", "box");
            spec.children.push_back(shapeNode("a", 10.0f, 10.0f));
            spec.children.push_back(shapeNode("b", 10.0f, 10.0f));

            EntityRef root = buildTree(&ecs, spec);
            settle(ecs);

            // Both children were added to the layout, not anchored as siblings.
            auto layout = lastBoxLayout->get<VerticalLayout>();
            EXPECT_EQ(layout->entities.size(), 2u);

            // They are still tracked and named on the wrap prefab.
            auto prefab = root->get<Prefab>();
            EXPECT_EQ(prefab->childrenIds.size(), 3u);   // main + 2 child wraps
            EXPECT_FALSE(prefab->getEntity("a").empty());
            EXPECT_FALSE(prefab->getEntity("b").empty());

            // No sibling anchors were applied to the child wraps.
            for (auto& wrap : childWraps(ecs, root))
            {
                if (wrap->has<UiAnchor>())
                {
                    auto a = wrap->get<UiAnchor>();
                    EXPECT_FALSE(a->hasTopAnchor);
                    EXPECT_FALSE(a->hasLeftAnchor);
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, child_defaults_merge_under_child_props)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec = node("Box", "box", {{"inner", 64.0f}, {"z", 10.0f}});
            spec.children.push_back(node("Probe", "inherits"));
            spec.children.push_back(node("Probe", "explicit", {{"width", 30.0f}}));

            EntityRef root = buildTree(&ecs, spec);

            // Read before the first frame: the sizes are what the factories set from their props.
            auto inherits = leafOf(root, "inherits")->get<PositionComponent>();
            auto explicitP = leafOf(root, "explicit")->get<PositionComponent>();

            // Inherited: the box's inner width and content band replace the schema defaults.
            EXPECT_FLOAT_EQ(inherits->width, 64.0f);
            EXPECT_FLOAT_EQ(inherits->z, 15.0f);

            // Explicit child value wins over the inherited one; z still inherited.
            EXPECT_FLOAT_EQ(explicitP->width, 30.0f);
            EXPECT_FLOAT_EQ(explicitP->z, 15.0f);

            // Schema defaults still fill what neither gave.
            EXPECT_FLOAT_EQ(inherits->height, 1.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, child_defaults_forward_through_layouts_and_bare_wraps)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec = node("Box", "box", {{"inner", 48.0f}});

            NodeSpec column = node("Layout:Vertical", "column");
            column.children.push_back(node("Probe", "inColumn"));

            NodeSpec group = node("", "group");
            group.children.push_back(node("Probe", "inGroup"));

            spec.children.push_back(column);
            spec.children.push_back(group);

            EntityRef root = buildTree(&ecs, spec);

            // A layout has no prefab: its children's names land on the enclosing one.
            EntityRef a = leafOf(root, "inColumn");
            ASSERT_FALSE(a.empty());
            EXPECT_FLOAT_EQ(a->get<PositionComponent>()->width, 48.0f);

            // A bare wrap is a prefab of its own: its children are named on it.
            EntityRef groupEnt = leafOf(root, "group");
            ASSERT_FALSE(groupEnt.empty());
            EntityRef b = groupEnt->get<Prefab>()->getEntity("inGroup");
            ASSERT_FALSE(b.empty());
            EXPECT_FLOAT_EQ(b->get<PositionComponent>()->width, 48.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, x_and_y_props_place_the_wrap)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec = shapeNode("bg", 100.0f, 80.0f);
            spec.props["x"] = 40.0f;
            spec.props["y"] = 25.0f;

            EntityRef root = buildTree(&ecs, spec);
            settle(ecs);

            auto container = root->get<PositionComponent>();
            EXPECT_FLOAT_EQ(container->x, 40.0f);
            EXPECT_FLOAT_EQ(container->y, 25.0f);

            auto leaf = leafOf(root, "bg")->get<PositionComponent>();
            EXPECT_FLOAT_EQ(leaf->x, 40.0f);
            EXPECT_FLOAT_EQ(leaf->y, 25.0f);
            EXPECT_FLOAT_EQ(leaf->width, 100.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, records_reach_the_factory)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec = node("Rec", "r");
            spec.records["items"] = {
                {{"label", "a"}, {"n", 1}},
                {{"label", "b"}, {"n", 2}},
            };

            EntityRef root = buildTree(&ecs, spec);

            EXPECT_FLOAT_EQ(leafOf(root, "r")->get<PositionComponent>()->width, 2.0f);
        }

        // ----------------------------------------------------------------------------------------
        // A Required param that the node does not give: the factory is never called, the node
        // becomes a bare wrap (no main entity), and the problem is logged once.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, missing_required_param_refuses_the_leaf)
        {
            MockLogger<CaptureSink> logger;
            EntitySystem ecs;
            bootstrap(ecs);

            CaptureSink::messages().clear();
            EntityRef root = buildTree(&ecs, node("Need", "need", {{"width", 9.0f}}));

            ASSERT_FALSE(root.empty());
            EXPECT_TRUE(leafOf(root, "MainEntity").empty());
            EXPECT_EQ(countLogs("missing required parameter: 'label'"), 1u) << allLogs();

            // Direct registry path agrees.
            auto* registry = ecs.getSystem<PrefabFactoryRegistry>();
            EXPECT_TRUE(registry->build("Need", {{"width", 9.0f}}).empty());

            // Given the param, the same node builds and the schema default still fills `width`.
            CaptureSink::messages().clear();
            EntityRef ok = buildTree(&ecs, node("Need", "need", {{"label", "yes"}}));

            ASSERT_FALSE(leafOf(ok, "MainEntity").empty());
            EXPECT_FLOAT_EQ(leafOf(ok, "need")->get<PositionComponent>()->width, 4.0f);
            EXPECT_EQ(countLogs("missing required parameter"), 0u) << allLogs();
        }

        // ----------------------------------------------------------------------------------------
        // A slot that is not a layout is refused: logged, and the children fall back to the
        // sibling path (here with Flow::Vertical, so they get the synthesised anchors).
        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, slot_without_layout_falls_back_to_sibling_anchoring)
        {
            MockLogger<CaptureSink> logger;
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec = node("BadSlot", "bad");
            spec.flow = Flow::Vertical;
            spec.children.push_back(shapeNode("a", 10.0f, 10.0f));
            spec.children.push_back(shapeNode("b", 10.0f, 10.0f));

            CaptureSink::messages().clear();
            EntityRef root = buildTree(&ecs, spec);
            settle(ecs);

            EXPECT_EQ(countLogs("returned a slot without a layout component"), 1u) << allLogs();

            auto wraps = childWraps(ecs, root);
            ASSERT_EQ(wraps.size(), 2u);

            // Flow synthesis ran: every child wrap is anchored (first to main, second to the first).
            for (auto& wrap : wraps)
            {
                ASSERT_TRUE(wrap->has<UiAnchor>());
                EXPECT_TRUE(wrap->get<UiAnchor>()->hasTopAnchor);
                EXPECT_TRUE(wrap->get<UiAnchor>()->hasLeftAnchor);
            }

            // And the column actually stacks: b sits below a.
            auto a = leafOf(root, "a")->get<PositionComponent>();
            auto b = leafOf(root, "b")->get<PositionComponent>();
            EXPECT_FLOAT_EQ(b->y, a->y + a->height);
        }

        // ----------------------------------------------------------------------------------------
        // Anchors on a slot child are ignored (the layout places it) and reported once, however
        // many children carry them.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, anchors_on_slot_children_are_ignored_and_warned_once)
        {
            MockLogger<CaptureSink> logger;
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec spec = node("Box", "box");

            NodeSpec a = shapeNode("a", 10.0f, 10.0f);
            a.anchors = {AnchorSpec{"main", AnchorType::Left, 30.0f}};
            NodeSpec b = shapeNode("b", 10.0f, 10.0f);
            b.anchors = {AnchorSpec{"main", AnchorType::Top, 30.0f}};
            spec.children.push_back(a);
            spec.children.push_back(b);

            CaptureSink::messages().clear();
            EntityRef root = buildTree(&ecs, spec);
            settle(ecs);

            EXPECT_EQ(countLogs("their anchors are ignored"), 1u) << allLogs();
            EXPECT_EQ(countLogs("Anchor target not found"), 0u) << allLogs();

            // Both are in the layout and none of the anchors was applied.
            EXPECT_EQ(lastBoxLayout->get<VerticalLayout>()->entities.size(), 2u);

            for (auto& wrap : childWraps(ecs, root))
            {
                ASSERT_TRUE(wrap->has<UiAnchor>());
                EXPECT_FALSE(wrap->get<UiAnchor>()->hasTopAnchor);
                EXPECT_FALSE(wrap->get<UiAnchor>()->hasLeftAnchor);
            }
        }

        // ----------------------------------------------------------------------------------------
        // A layout root is wrapped like every other node: the root carries a Prefab whose main
        // entity is the layout, the placement goes on the container, the layout's named children
        // resolve through the root, and the container follows the layout's size.
        // ----------------------------------------------------------------------------------------
        TEST(prefab_tree_test, layout_root_is_wrapped_in_a_prefab)
        {
            EntitySystem ecs;
            bootstrap(ecs);

            NodeSpec column = node("Layout:Vertical", "column", {{"x", 10.0f}, {"y", 20.0f}, {"spacing", 4}});
            column.children.push_back(shapeNode("a", 30.0f, 10.0f));
            column.children.push_back(shapeNode("b", 50.0f, 10.0f));

            EntityRef root = buildTree(&ecs, column);
            settle(ecs);
            settle(ecs);

            ASSERT_FALSE(root.empty());
            ASSERT_TRUE(root->has<Prefab>());

            EntityRef layout = leafOf(root, "MainEntity");
            ASSERT_FALSE(layout.empty());
            EXPECT_TRUE(layout->has<VerticalLayout>());
            EXPECT_EQ(layout->get<VerticalLayout>()->entities.size(), 2u);
            EXPECT_EQ(leafOf(root, "column").id, layout.id);

            EXPECT_FALSE(leafOf(root, "a").empty());
            EXPECT_FALSE(leafOf(root, "b").empty());

            auto container = root->get<PositionComponent>();
            EXPECT_FLOAT_EQ(container->x, 10.0f);
            EXPECT_FLOAT_EQ(container->y, 20.0f);

            // The container took the layout's size, which the layout took from its rows.
            auto layoutPos = layout->get<PositionComponent>();
            EXPECT_GT(layoutPos->width, 0.0f);
            EXPECT_FLOAT_EQ(container->width, layoutPos->width);
            EXPECT_FLOAT_EQ(container->height, layoutPos->height);
        }
    }
}
