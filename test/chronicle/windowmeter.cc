#include "stdafx.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "UI/windowmeter.h"
#include "Core/motion.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "Systems/gamefacts.h"
#include "Systems/tween.h"
#include "Systems/coresystems.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"

#include "mocklogger.h"
#include "factfeed.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            struct WindowMeterFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;
                WorldFacts* facts = nullptr;
                FactFeed* feed = nullptr;

                WindowMeterFixture()
                {
                    Motion::setReduced(true);
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<TweenSystem>();
                    facts = createTestFacts(&ecs);
                    feed = ecs.createSystem<FactFeed>();
                    theme = ecs.createSystem<ThemeSystem>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                    installIconEntries();
                    ecs.sendEvent(ResizeEvent{1320.0f, 860.0f});
                }

                void installIconEntries()
                {
                    std::vector<IconEntry> marks;
                    const int sizes[5] = {14, 16, 18, 24, 48};

                    for (const auto& name : markNames())
                    {
                        for (int s : sizes)
                            marks.push_back({name, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    }

                    icons->registerEntriesForTest("chronicle", marks, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                WindowMeter make(const WindowMeterSpec& spec, float x = 100.0f, float y = 100.0f)
                {
                    WindowMeter meter = makeWindowMeter(&ecs, spec);
                    meter.root->get<PositionComponent>()->setX(x);
                    meter.root->get<PositionComponent>()->setY(y);
                    settle();

                    return meter;
                }

                float asc(const std::string& style)
                {
                    const TextStyle& s = theme->style(style);

                    return ttf->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
                }

                constant::Vector4D color(const std::string& token, const std::string& id) { return theme->theme().color(token, id); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }

                float left(EntityRef e, const WindowMeter& meter) { return pos(e)->x - pos(meter.root)->x; }

                float top(EntityRef e, const WindowMeter& meter) { return pos(e)->y - pos(meter.root)->y; }

                float right(EntityRef e, const WindowMeter& meter) { return left(e, meter) + pos(e)->width; }

                std::string element(EntityRef e) { return ecs.getEntity(e.id)->get<ThemeComponent>()->element; }

                std::string iconName(const Mark& mark) { return ecs.getEntity(mark.entity.id)->get<IconComponent>()->iconName; }
            };

            WindowMeterSpec squireSpec(float age = 20.2f)
            {
                WindowMeterSpec spec;
                spec.width = 288.0f;
                spec.name = "Squire";
                spec.from = 16.0f;
                spec.to = 22.0f;
                spec.age = age;
                spec.state = WindowState::Open;
                spec.note = "Open 22 more months. One attempt fits; two do not.";

                return spec;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(windowmeter_test, geometry)
        {
            MockLogger logger;
            WindowMeterFixture s;

            WindowMeter meter = s.make(squireSpec());

            EXPECT_FLOAT_EQ(s.pos(meter.root)->width, 288.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.root)->height, 54.0f);
            EXPECT_FLOAT_EQ(meter.height(&s.ecs), 54.0f);

            // Head: the gate, the name 8 px after it, the range at the right on the name's baseline
            EXPECT_EQ(meter.mark.spec.name, "gate");
            EXPECT_EQ(s.iconName(meter.mark), "gate");
            EXPECT_EQ(meter.mark.spec.size, MarkSize::S16);
            EXPECT_EQ(s.element(meter.mark.entity), "window.mark");
            EXPECT_NEAR(s.left(meter.mark.entity, meter), 0.0f, 0.01f);
            EXPECT_NEAR(s.top(meter.mark.entity, meter), 4.0f, 0.01f);

            EXPECT_EQ(meter.name.spec.text, "Squire");
            EXPECT_EQ(meter.name.spec.style, "tab");
            EXPECT_EQ(meter.name.spec.overflow, Overflow::Ellipsis);
            EXPECT_EQ(s.element(meter.name.entity), "window.name");
            EXPECT_NEAR(s.left(meter.name.entity, meter), 24.0f, 0.01f);

            EXPECT_EQ(meter.range.spec.text, "16\xE2\x80\x93" "22");
            EXPECT_EQ(s.element(meter.range.entity), "window.range");
            EXPECT_NEAR(s.right(meter.range.entity, meter), 288.0f, 0.01f);
            EXPECT_NEAR(s.right(meter.name.entity, meter), s.left(meter.range.entity, meter) - 8.0f, 0.01f);

            const float nameBaseline = s.pos(meter.name.entity)->y + s.asc("tab");
            const float rangeBaseline = s.pos(meter.range.entity)->y + s.asc("control");
            EXPECT_NEAR(nameBaseline, rangeBaseline, 0.5f);

            // The small rule, no nib, filled by 20.2 over 16-22
            EXPECT_NEAR(s.top(meter.rule.root, meter), 28.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(meter.rule.root)->height, 6.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.rule.root)->width, 288.0f);
            EXPECT_TRUE(meter.rule.spec.small);
            EXPECT_FALSE(meter.rule.nib.has_value());
            EXPECT_NEAR(meter.rule.shown, 70.0f, 0.001f);

            // The note under the rule: rule bottom + 4
            EXPECT_NEAR(s.top(meter.note.entity, meter), 38.0f, 0.01f);
            EXPECT_EQ(meter.note.spec.style, "tick");
            EXPECT_EQ(meter.note.spec.overflow, Overflow::Wrap);
            EXPECT_EQ(s.element(meter.note.entity), "window.note");
            EXPECT_FLOAT_EQ(s.pos(meter.note.entity)->height, 16.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(windowmeter_test, fill_clamps)
        {
            MockLogger logger;
            WindowMeterFixture s;

            WindowMeter early = s.make(squireSpec(10.0f));
            EXPECT_FLOAT_EQ(early.rule.shown, 0.0f);

            WindowMeter late = s.make(squireSpec(30.0f), 100.0f, 200.0f);
            EXPECT_FLOAT_EQ(late.rule.shown, 100.0f);

            WindowMeter meter = s.make(squireSpec(), 100.0f, 300.0f);
            meter.setRange(&s.ecs, 18.0f, 24.0f);
            s.settle();

            EXPECT_NEAR(meter.rule.shown, 36.67f, 0.05f);
            EXPECT_EQ(meter.range.spec.text, "18\xE2\x80\x93" "24");
            EXPECT_NEAR(s.right(meter.range.entity, meter), 288.0f, 0.01f);

            // Ages that are not whole keep one decimal
            EXPECT_EQ(windowRangeText(9.5f, 13.0f), "9.5\xE2\x80\x93" "13");

            meter.setAge(&s.ecs, 30.0f, false);
            EXPECT_FLOAT_EQ(meter.rule.shown, 100.0f);
            meter.setAge(&s.ecs, 0.0f, false);
            EXPECT_FLOAT_EQ(meter.rule.shown, 0.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(windowmeter_test, states_are_elements)
        {
            MockLogger logger;
            WindowMeterFixture s;

            WindowMeter meter = s.make(squireSpec());

            EXPECT_EQ(s.element(meter.name.entity), "window.name");
            EXPECT_EQ(s.element(meter.mark.entity), "window.mark");
            EXPECT_EQ(s.element(meter.note.entity), "window.note");

            meter.setState(&s.ecs, WindowState::Upcoming);
            s.settle();
            EXPECT_EQ(s.element(meter.name.entity), "window.name.upcoming");
            EXPECT_EQ(s.element(meter.mark.entity), "window.mark.upcoming");
            EXPECT_EQ(s.element(meter.note.entity), "window.note.upcoming");
            EXPECT_EQ(s.iconName(meter.mark), "gate");

            meter.setState(&s.ecs, WindowState::Closed);
            s.settle();
            EXPECT_EQ(s.element(meter.name.entity), "window.name.closed");
            EXPECT_EQ(s.element(meter.mark.entity), "window.mark.closed");
            EXPECT_EQ(s.element(meter.note.entity), "window.note.closed");
            EXPECT_EQ(s.iconName(meter.mark), "cross");

            // The range never changes colour
            EXPECT_EQ(s.element(meter.range.entity), "window.range");

            meter.setState(&s.ecs, WindowState::Open);
            s.settle();
            EXPECT_EQ(s.element(meter.name.entity), "window.name");
            EXPECT_EQ(s.element(meter.mark.entity), "window.mark");
            EXPECT_EQ(s.element(meter.note.entity), "window.note");
            EXPECT_EQ(s.iconName(meter.mark), "gate");

            // Built closed: the cross from the start
            WindowMeterSpec closed = squireSpec();
            closed.state = WindowState::Closed;
            WindowMeter choir = s.make(closed, 100.0f, 300.0f);
            EXPECT_EQ(s.iconName(choir.mark), "cross");
            EXPECT_EQ(s.element(choir.name.entity), "window.name.closed");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(windowmeter_test, note_wraps_and_grows)
        {
            MockLogger logger;
            WindowMeterFixture s;

            WindowMeter meter = s.make(squireSpec());
            EXPECT_FLOAT_EQ(meter.height(&s.ecs), 54.0f);

            const std::string longNote = "Open 22 more months. One attempt fits; two do not. The Guild writes again when you are 19.";
            ASSERT_EQ(longNote.size(), 90u);

            meter.setNote(&s.ecs, longNote);
            s.settle();

            EXPECT_EQ(meter.spec.note, longNote);
            EXPECT_FLOAT_EQ(s.pos(meter.note.entity)->height, 32.0f);
            EXPECT_FLOAT_EQ(meter.height(&s.ecs), 38.0f + 32.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.note.entity)->width, 288.0f);

            // Back to one line: the root shrinks with it
            meter.setNote(&s.ecs, "One attempt fits.");
            s.settle();
            EXPECT_FLOAT_EQ(meter.height(&s.ecs), 54.0f);

            // An empty note keeps its line
            meter.setNote(&s.ecs, "");
            s.settle();
            EXPECT_FLOAT_EQ(meter.height(&s.ecs), 54.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(windowmeter_test, fed_from_worldfacts)
        {
            MockLogger logger;
            WindowMeterFixture s;

            WindowMeter meter = s.make(squireSpec(17.0f));

            s.feed->onFact = [&](const std::string& name, const ElementType& v) {
                if (name == "life.age")
                {
                    meter.setAge(&s.ecs, v.get<float>());
                }
                else if (name == "window.squire.state")
                {
                    const std::string state = v.get<std::string>();
                    meter.setState(&s.ecs, state == "closed" ? WindowState::Closed : state == "upcoming" ? WindowState::Upcoming : WindowState::Open);
                }
                else if (name == "window.squire.note")
                {
                    meter.setNote(&s.ecs, v.get<std::string>());
                }
            };

            s.facts->setFact("life.age", 20.2f);
            s.facts->setFact("window.squire.state", std::string("closed"));
            s.facts->setFact("window.squire.note", std::string("The Squire door is shut."));

            // The update leaves on the first pass and is delivered on the next one
            s.settle();
            s.settle();

            EXPECT_FLOAT_EQ(meter.spec.age, 20.2f);
            EXPECT_NEAR(meter.rule.shown, 70.0f, 0.001f);
            EXPECT_EQ(meter.spec.state, WindowState::Closed);
            EXPECT_EQ(s.element(meter.name.entity), "window.name.closed");
            EXPECT_EQ(s.iconName(meter.mark), "cross");
            EXPECT_EQ(meter.note.spec.text, "The Squire door is shut.");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(windowmeter_test, theme_switch)
        {
            MockLogger logger;
            WindowMeterFixture s;

            WindowMeterSpec spec = squireSpec();
            spec.state = WindowState::Closed;
            WindowMeter meter = s.make(spec);

            s.theme->setTheme("candle");
            s.settle();

            // state-locked is an alias of ink-faint
            const auto name = s.ecs.getEntity(meter.name.entity.id)->get<TTFText>()->colors;
            EXPECT_FLOAT_EQ(name.x, s.color("ink-faint", "candle").x);
            EXPECT_FLOAT_EQ(name.y, s.color("ink-faint", "candle").y);
            EXPECT_FLOAT_EQ(name.z, s.color("ink-faint", "candle").z);

            const auto note = s.ecs.getEntity(meter.note.entity.id)->get<TTFText>()->colors;
            EXPECT_FLOAT_EQ(note.x, s.color("ink-faint", "candle").x);
            EXPECT_FLOAT_EQ(note.y, s.color("ink-faint", "candle").y);
            EXPECT_FLOAT_EQ(note.z, s.color("ink-faint", "candle").z);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(windowmeter_test, z_bands)
        {
            MockLogger logger;
            WindowMeterFixture s;

            WindowMeterSpec spec = squireSpec();
            spec.z = 20;
            WindowMeter meter = s.make(spec);

            EXPECT_FLOAT_EQ(s.pos(meter.root)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.mark.entity)->z, 21.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.name.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.range.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.note.entity)->z, 22.0f);
            EXPECT_FLOAT_EQ(s.pos(meter.rule.root)->z, 21.0f);
            EXPECT_LE(s.pos(meter.rule.frame)->z, 29.0f);

            for (auto e : {meter.root, meter.mark.entity, meter.name.entity, meter.range.entity, meter.note.entity, meter.rule.root, meter.rule.fill})
            {
                const float z = s.pos(e)->z;
                EXPECT_FLOAT_EQ(z, static_cast<float>(static_cast<int>(z)));
            }
        }
    }
}
