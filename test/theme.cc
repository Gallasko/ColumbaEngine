#include "stdafx.h"

#include <cmath>

#include <gtest/gtest.h>

#include "UI/themesystem.h"
#include "UI/ttftext.h"
#include "ECS/entitysystem.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/prefabspec.h"
#include "UI/prefabbuilder.h"
#include "UI/prefabfactory.h"
#include "UI/enginefactories.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // A user drawable with its own themable pairing.
        struct Glow : public Component
        {
            DEFAULT_COMPONENT_MEMBERS(Glow)

            inline static std::string getType() { return "Glow"; }

            float level = 0.0f;
        };

        struct GlowSystem : public System<Own<Glow>>
        {
            virtual std::string getSystemName() const override { return "Glow System"; }
        };
    }

    // An owned component needs its archive form, as any hand-written component does
    template <>
    void serialize(Archive& archive, const test::Glow& value)
    {
        archive.startSerialization(test::Glow::getType());

        serialize(archive, "level", value.level);

        archive.endSerialization();
    }

    template <>
    test::Glow deserialize(const UnserializedObject& serializedString)
    {
        test::Glow value;

        defaultDeserialize(serializedString, "level", value.level);

        return value;
    }

    namespace test
    {
        namespace
        {
            void expectColor(const constant::Vector4D& c, float r, float g, float b, float a)
            {
                EXPECT_FLOAT_EQ(c.x, r);
                EXPECT_FLOAT_EQ(c.y, g);
                EXPECT_FLOAT_EQ(c.z, b);
                EXPECT_FLOAT_EQ(c.w, a);
            }

            void expectColor(const constant::Vector4D& a, const constant::Vector4D& b)
            {
                expectColor(a, b.x, b.y, b.z, b.w);
            }

            bool isMagenta(const constant::Vector4D& c)
            {
                return std::abs(c.x - 255.0f) < 0.5f and std::abs(c.y) < 0.5f and std::abs(c.z - 255.0f) < 0.5f and std::abs(c.w - 255.0f) < 0.5f;
            }

            // The fixture theme, loaded once (read from beside the binary).
            const Theme& basic()
            {
                static Theme theme = Theme::load("theme/basic.json");
                return theme;
            }

            // Records the last ThemeChangedEvent.
            struct ThemeSpy : public System<Listener<ThemeChangedEvent>>
            {
                virtual std::string getSystemName() const override { return "Theme Spy"; }

                virtual void onEvent(const ThemeChangedEvent& event) override { last = event.theme; ++count; }

                std::string last;
                int count = 0;
            };

            // Engine render systems + the theme system + a TTFTextSystem, in either creation order.
            struct ThemeFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                ThemeSystem* theme = nullptr;
                TTFTextSystem* ttf = nullptr;
                ThemeSpy* spy = nullptr;

                ThemeFixture(bool textFirst = false)
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<RoundedRect2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);

                    if (textFirst)
                        ttf = ecs.createSystem<TTFTextSystem>(&renderer);

                    theme = ecs.createSystem<ThemeSystem>();
                    spy = ecs.createSystem<ThemeSpy>();

                    if (not textFirst)
                        ttf = ecs.createSystem<TTFTextSystem>(&renderer);

                    theme->loadTheme("theme/basic.json", "fonts");
                }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, loads_fixture)
        {
            const Theme& t = basic();

            EXPECT_TRUE(t.ok());
            EXPECT_TRUE(t.errors().empty());
            EXPECT_EQ(t.version(), 1);
            EXPECT_EQ(t.name(), "Basic");

            ASSERT_EQ(t.themeIds().size(), 2u);
            EXPECT_EQ(t.themeIds()[0], "day");
            EXPECT_EQ(t.themeIds()[1], "night");
            EXPECT_EQ(t.themeIndex("night"), 1);
            EXPECT_EQ(t.themeIndex("dusk"), -1);

            EXPECT_EQ(t.colorNames().size(), 5u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, colors_per_theme_scalar_and_default)
        {
            const Theme& t = basic();

            expectColor(t.color("paper", "day"), 255, 255, 255, 255);
            expectColor(t.color("paper", "night"), 16, 16, 16, 255);

            // A scalar value applies to every theme
            expectColor(t.color("accent", "day"), 255, 128, 0, 255);
            expectColor(t.color("accent", "night"), 255, 128, 0, 255);

            // "default" covers the themes without an entry, aliases resolve, rgba() rounds the alpha
            expectColor(t.color("accent-soft", "day"), 255, 128, 0, 255);
            expectColor(t.color("accent-soft", "night"), 255, 128, 0, 128);
            expectColor(t.color("link", "night"), t.color("accent", "night"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, scales_per_theme)
        {
            const Theme& t = basic();

            EXPECT_FLOAT_EQ(t.space(1, "day"), 4.0f);
            EXPECT_FLOAT_EQ(t.spacing("space-2", "day"), 8.0f);
            EXPECT_FLOAT_EQ(t.spacing("space-2", "night"), 10.0f);
            EXPECT_FLOAT_EQ(t.radius("radius-md", "night"), 6.0f);
            EXPECT_FLOAT_EQ(t.border("border-rule", "day"), 2.0f);
            EXPECT_FLOAT_EQ(t.opacity("opacity-soft", "day"), 0.5f);
            EXPECT_TRUE(t.hasScale("radius-md"));
            EXPECT_FALSE(t.hasScale("radius-xl"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, styles_and_per_theme_alias)
        {
            const Theme& t = basic();

            ASSERT_EQ(t.styles("day").size(), 2u);

            // Identical in every theme: one atlas named after the style
            EXPECT_EQ(t.style("body", "day").fontAlias, "body");
            EXPECT_EQ(t.style("body", "night").fontAlias, "body");
            EXPECT_EQ(t.style("body", "day").sizePx, 16);
            EXPECT_EQ(t.style("body", "day").lineHeightPx, 24);
            EXPECT_EQ(t.style("body", "day").fontFile, "Inter-Regular.ttf");

            // Overridden at night: one atlas per theme
            EXPECT_EQ(t.style("caption", "day").sizePx, 12);
            EXPECT_EQ(t.style("caption", "night").sizePx, 14);
            EXPECT_EQ(t.style("caption", "day").fontAlias, "caption@day");
            EXPECT_EQ(t.style("caption", "night").fontAlias, "caption@night");
            EXPECT_FLOAT_EQ(t.style("caption", "day").letterSpacingPx, 0.5f);
            EXPECT_FLOAT_EQ(t.style("caption", "night").letterSpacingPx, 0.75f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, unknown_style_falls_back_to_body)
        {
            MockLogger logger;
            const Theme& t = basic();

            EXPECT_FALSE(t.hasStyle("nope"));
            EXPECT_EQ(t.style("nope", "day").name, "body");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, alias_cycle_is_error)
        {
            MockLogger logger;

            const std::string json = R"({"version":1,"color":{"tokens":[
                {"name":"a","value":"{b}"},
                {"name":"b","value":"{a}"}
            ]}})";

            Theme t = Theme::parse(json);

            EXPECT_FALSE(t.ok());
            ASSERT_EQ(t.themeIds().size(), 1u);
            EXPECT_EQ(t.themeIds()[0], "default");
            EXPECT_TRUE(isMagenta(t.color("a", "default")));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, missing_theme_value_is_error)
        {
            const std::string json = R"({"themes":["day","night"],"color":{"tokens":[
                {"name":"a","value":{"day":"#000000"}}
            ]}})";

            Theme t = Theme::parse(json);

            EXPECT_FALSE(t.ok());
            EXPECT_FALSE(t.hasColor("a"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, unknown_color_is_magenta_and_logged_once)
        {
            MockLogger logger;

            Theme t = Theme::parse(R"({"color":{"tokens":[{"name":"a","value":"#000000"}]}})");

            EXPECT_TRUE(isMagenta(t.color("nope", "default")));
            EXPECT_TRUE(isMagenta(t.color("nope", "default")));
            EXPECT_EQ(logger.getNbError(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, element_prefix_chain)
        {
            const Theme& t = basic();
            bool found = false;

            ElementMap hover = t.resolveElement("button.ground.hover", "day", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(hover.at("color").get<std::string>(), "accent-soft");
            EXPECT_FLOAT_EQ(hover.at("radius").get<float>(), 3.0f);

            ElementMap hoverNight = t.resolveElement("button.ground.hover", "night", found);
            EXPECT_EQ(hoverNight.at("color").get<std::string>(), "ink");

            // "soft" is a root element mixed in by its segment
            ElementMap caption = t.resolveElement("label.caption.soft", "day", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(caption.at("font").get<std::string>(), "caption");
            EXPECT_EQ(caption.at("color").get<std::string>(), "ink");
            EXPECT_EQ(caption.at("alpha").get<std::string>(), "opacity-soft");

            // An undefined leaf falls back to its defined prefix
            ElementMap pressed = t.resolveElement("button.ground.pressed", "day", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(pressed.at("color").get<std::string>(), "accent");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, element_segment_rules)
        {
            const Theme& t = basic();
            bool found = false;

            // A color token as the last segment paints with it
            ElementMap muted = t.resolveElement("label.caption.paper", "day", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(muted.at("font").get<std::string>(), "caption");
            EXPECT_EQ(muted.at("color").get<std::string>(), "paper");

            // A root element as the last segment is mixed in
            ElementMap dim = t.resolveElement("panel.ground.dim", "night", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(dim.at("color").get<std::string>(), "paper");
            EXPECT_FLOAT_EQ(dim.at("alpha").get<float>(), 0.5f);

            // A bare color token is an element of its own
            ElementMap bare = t.resolveElement("accent", "day", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(bare.at("color").get<std::string>(), "accent");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, element_extends)
        {
            MockLogger logger;
            const Theme& t = basic();
            bool found = false;

            ElementMap disabled = t.resolveElement("button.disabled", "day", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(disabled.at("color").get<std::string>(), "accent");
            EXPECT_FLOAT_EQ(disabled.at("radius").get<float>(), 3.0f);
            EXPECT_FLOAT_EQ(disabled.at("alpha").get<float>(), 0.25f);

            // A cyclic extends chain stops and keeps the element's own entries
            ElementMap loop = t.resolveElement("loop-a", "day", found);
            EXPECT_TRUE(found);
            EXPECT_EQ(loop.at("color").get<std::string>(), "ink");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, unknown_element_is_empty)
        {
            const Theme& t = basic();
            bool found = true;

            ElementMap none = t.resolveElement("ghost.part", "day", found);

            EXPECT_FALSE(found);
            EXPECT_TRUE(none.empty());
            EXPECT_TRUE(t.hasElement("label"));
            EXPECT_FALSE(t.hasElement("ghost"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, unknown_element_extends_is_error)
        {
            Theme t = Theme::parse(R"({"elements":{"a":{"extends":"ghost","color":"#000000"}}})");

            EXPECT_FALSE(t.ok());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, attach_paints_shape)
        {
            MockLogger logger;
            ThemeFixture s;

            auto shape = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 10.0f, 10.0f);
            s.ecs.attach<ThemeComponent>(shape.entity, "panel.ground");
            s.ecs.executeOnce();

            expectColor(shape.entity->get<Simple2DObject>()->colors, 255, 255, 255, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, component_can_attach_before_the_drawable)
        {
            MockLogger logger;
            ThemeFixture s;

            auto ent = s.ecs.createEntity();
            s.ecs.attach<ThemeComponent>(ent, "button.ground");
            s.ecs.attach<Simple2DObject>(ent, Shape2D::Square);
            s.ecs.executeOnce();

            expectColor(ent->get<Simple2DObject>()->colors, 255, 128, 0, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, set_element_repaints)
        {
            MockLogger logger;
            ThemeFixture s;

            auto shape = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 10.0f, 10.0f);
            s.ecs.attach<ThemeComponent>(shape.entity, "panel.ground");
            s.ecs.executeOnce();

            shape.entity->get<ThemeComponent>()->setElement("button.ground");
            s.ecs.executeOnce();

            EXPECT_EQ(shape.entity->get<ThemeComponent>()->element, "button.ground");
            expectColor(shape.entity->get<Simple2DObject>()->colors, 255, 128, 0, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, set_theme_repaints_all_and_broadcasts)
        {
            MockLogger logger;
            ThemeFixture s;

            auto shape = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 10.0f, 10.0f);
            s.ecs.attach<ThemeComponent>(shape.entity, "panel.ground");

            auto text = s.theme->makeText(&s.ecs, "label", "x");
            s.ecs.executeOnce();

            EXPECT_EQ(s.theme->currentTheme(), "day");
            expectColor(text.get<TTFText>()->colors, 32, 32, 32, 255);

            s.theme->setTheme("night");
            EXPECT_EQ(s.theme->currentTheme(), "night");
            expectColor(s.theme->color("paper"), 16, 16, 16, 255);
            s.ecs.executeOnce();

            expectColor(shape.entity->get<Simple2DObject>()->colors, 16, 16, 16, 255);
            expectColor(text.get<TTFText>()->colors, 224, 224, 224, 255);
            EXPECT_EQ(s.spy->last, "night");
            EXPECT_EQ(s.spy->count, 1);

            // Switching to the current theme is a no-op
            s.theme->setTheme("night");
            s.ecs.executeOnce();
            EXPECT_EQ(s.spy->count, 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, set_theme_event)
        {
            MockLogger logger;
            ThemeFixture s;

            auto shape = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 10.0f, 10.0f);
            s.ecs.attach<ThemeComponent>(shape.entity, "panel.ground");
            s.ecs.executeOnce();

            s.ecs.sendEvent(SetThemeEvent{"night"});
            s.ecs.executeOnce();
            s.ecs.executeOnce();

            EXPECT_EQ(s.theme->currentTheme(), "night");
            expectColor(shape.entity->get<Simple2DObject>()->colors, 16, 16, 16, 255);
            EXPECT_EQ(s.spy->last, "night");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, unknown_theme_is_ignored)
        {
            MockLogger logger;
            ThemeFixture s;

            const unsigned int before = logger.getNbError();

            s.theme->setTheme("dusk");

            EXPECT_EQ(s.theme->currentTheme(), "day");
            EXPECT_EQ(logger.getNbError(), before + 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, literal_color_and_alpha)
        {
            MockLogger logger;
            ThemeFixture s;

            auto shape = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 10.0f, 10.0f);
            s.ecs.attach<ThemeComponent>(shape.entity, "literal");
            s.ecs.executeOnce();

            expectColor(shape.entity->get<Simple2DObject>()->colors, 64, 128, 192, 127.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, opacity_token_as_alpha)
        {
            MockLogger logger;
            ThemeFixture s;

            auto text = s.theme->makeText(&s.ecs, "label.soft", "x");
            s.ecs.executeOnce();

            expectColor(text.get<TTFText>()->colors, 32, 32, 32, 127.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, type_prefixed_entry_overrides)
        {
            MockLogger logger;
            ThemeFixture s;

            auto text = s.theme->makeText(&s.ecs, "mixed", "x");
            s.ecs.attach<Simple2DObject>(text.entity, Shape2D::Square);
            s.ecs.executeOnce();

            expectColor(text.entity->get<Simple2DObject>()->colors, 255, 255, 255, 255);
            expectColor(text.get<TTFText>()->colors, 32, 32, 32, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, font_entry_follows_the_style)
        {
            MockLogger logger;
            ThemeFixture s;

            auto text = s.theme->makeText(&s.ecs, "label.caption", "x");
            auto body = s.theme->makeText(&s.ecs, "label", "x");
            s.ecs.executeOnce();

            EXPECT_EQ(text.get<TTFText>()->fontPath, "caption@day");
            EXPECT_FLOAT_EQ(text.get<TTFText>()->letterSpacing, 0.5f);
            EXPECT_EQ(body.get<TTFText>()->fontPath, "body");

            s.theme->setTheme("night");
            s.ecs.executeOnce();

            EXPECT_EQ(text.get<TTFText>()->fontPath, "caption@night");
            EXPECT_FLOAT_EQ(text.get<TTFText>()->letterSpacing, 0.75f);
            EXPECT_EQ(body.get<TTFText>()->fontPath, "body");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, fonts_registered_whichever_system_comes_first)
        {
            MockLogger logger;

            ThemeFixture themeFirst;
            EXPECT_NE(themeFirst.ttf->fonts.find("body"), themeFirst.ttf->fonts.end());
            EXPECT_GT(themeFirst.theme->style("body").lineSpacingPx, -100.0f);

            ThemeFixture textFirst(true);
            EXPECT_NE(textFirst.ttf->fonts.find("caption@night"), textFirst.ttf->fonts.end());

            auto text = textFirst.theme->makeText(&textFirst.ecs, "label", "x");
            textFirst.ecs.executeOnce();
            expectColor(text.get<TTFText>()->colors, 32, 32, 32, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, radius_and_border_entries)
        {
            MockLogger logger;
            ThemeFixture s;

            auto stroke = makeStrokeRect2DShape(&s.ecs, 50.0f, 50.0f, {0.0f, 0.0f, 0.0f, 255.0f});
            s.ecs.attach<ThemeComponent>(stroke.entity, "panel.frame");

            auto rounded = makeRoundedRect2DShape(&s.ecs, 0.0f, 10.0f, 10.0f, {0.0f, 0.0f, 0.0f, 255.0f});
            s.ecs.attach<ThemeComponent>(rounded.entity, "button.ground");
            s.ecs.executeOnce();

            expectColor(stroke.entity->get<StrokeRect2DObject>()->colors, 32, 32, 32, 255);
            EXPECT_FLOAT_EQ(stroke.entity->get<StrokeRect2DObject>()->strokeWidth, 2.0f);
            EXPECT_FLOAT_EQ(stroke.entity->get<StrokeRect2DObject>()->cornerRadius, 6.0f);
            EXPECT_FLOAT_EQ(rounded.entity->get<RoundedRect2DObject>()->cornerRadius, 3.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, decorated_shapes_repaint)
        {
            MockLogger logger;
            ThemeFixture s;

            auto hatch = makeHatchRect2DShape(&s.ecs, 100.0f, 10.0f, {0.0f, 0.0f, 0.0f, 255.0f});
            auto dotted = makeDottedLine2DShape(&s.ecs, 100.0f, {0.0f, 0.0f, 0.0f, 255.0f});
            s.ecs.attach<ThemeComponent>(hatch.entity, "panel.ground");
            s.ecs.attach<ThemeComponent>(dotted.entity, "panel.ground");
            s.ecs.executeOnce();

            s.theme->setTheme("night");
            s.ecs.executeOnce();

            expectColor(hatch.entity->get<HatchRect2DObject>()->colors, 16, 16, 16, 255);
            expectColor(dotted.entity->get<DottedLine2DObject>()->colors, 16, 16, 16, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, custom_themable)
        {
            MockLogger logger;
            ThemeFixture s;

            s.ecs.createSystem<GlowSystem>();

            s.theme->registerThemable<Glow>([](EntityRef entity, const ElementMap& map, const ThemeSystem& theme) {
                entity->get<Glow>()->level = theme.resolveAlpha(map, Glow::getType());
            });

            auto ent = s.ecs.createEntity();
            s.ecs.attach<Glow>(ent);
            s.ecs.attach<ThemeComponent>(ent, "button.disabled");
            s.ecs.executeOnce();

            EXPECT_FLOAT_EQ(ent->get<Glow>()->level, 0.25f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, node_theme_keyword_paints_the_leaf)
        {
            MockLogger logger;
            ThemeFixture s;

            s.ecs.createSystem<PrefabSystem>();
            s.ecs.succeed<PositionComponentSystem, PrefabSystem>();
            auto* registry = s.ecs.createSystem<PrefabFactoryRegistry>();
            registerEnginePrefabFactories(registry);

            NodeSpec node;
            node.kind = "Shape2D";
            node.name = "ground";
            node.theme = "panel.ground";
            node.props = {
                {"width",  10.0f},
                {"height", 10.0f},
            };

            EntityRef root = buildNode(&s.ecs, node);
            s.ecs.executeOnce();
            s.ecs.executeOnce();

            ASSERT_TRUE(root);
            EntityRef leaf = root->get<Prefab>()->getEntity("ground");
            ASSERT_TRUE(leaf);
            ASSERT_TRUE(leaf->has<ThemeComponent>());
            EXPECT_EQ(leaf->get<ThemeComponent>()->element, "panel.ground");
            expectColor(leaf->get<Simple2DObject>()->colors, 255, 255, 255, 255);

            s.theme->setTheme("night");
            s.ecs.executeOnce();

            expectColor(leaf->get<Simple2DObject>()->colors, 16, 16, 16, 255);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, unthemable_entity_warns_once)
        {
            MockLogger logger;
            ThemeFixture s;

            const unsigned int before = logger.getNbWarning();

            auto ent = s.ecs.createEntity();
            s.ecs.attach<PositionComponent>(ent);
            s.ecs.attach<ThemeComponent>(ent, "label");
            s.ecs.executeOnce();

            s.theme->repaintAll();
            s.theme->repaintAll();

            EXPECT_EQ(logger.getNbWarning(), before + 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(theme_test, unknown_element_key_logs_once)
        {
            MockLogger logger;
            ThemeFixture s;

            auto shape = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 10.0f, 10.0f, {1.0f, 2.0f, 3.0f, 255.0f});
            s.ecs.attach<ThemeComponent>(shape.entity, "ghost");
            s.ecs.executeOnce();

            const unsigned int errors = logger.getNbError();

            s.theme->repaintAll();

            // Nothing to apply: the drawable keeps its colour, the key is reported once
            expectColor(shape.entity->get<Simple2DObject>()->colors, 1, 2, 3, 255);
            EXPECT_EQ(logger.getNbError(), errors);
            EXPECT_GE(errors, 1u);
        }
    }
}
